#include "client_app.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace mps::client
{
	PageWindow::PageWindow(qint64 tabId, QString title, QWidget* parent)
		: QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
		, tabId_(tabId)
	{
		setAttribute(Qt::WA_DeleteOnClose, false);
		setAttribute(Qt::WA_NativeWindow);
		setWindowTitle(title);
		setMinimumSize(0, 0);
		resize(640, 480);

		const QColor bg = (tabId % 2 == 0) ? QColor(255, 230, 230) : QColor(235, 230, 255);
		auto* root = new QWidget(this);
		auto* outer = new QVBoxLayout(this);
		outer->setContentsMargins(0, 0, 0, 0);
		outer->setSpacing(0);
		outer->addWidget(root);
		root->setStyleSheet(QStringLiteral("background:%1;").arg(bg.name()));
		auto* lay = new QVBoxLayout(root);
		lay->setContentsMargins(0, 0, 0, 0);
		auto* label = new QLabel(title, root);
		label->setAlignment(Qt::AlignCenter);
		QFont f = label->font();
		f.setPointSize(16);
		f.setBold(true);
		label->setFont(f);
		auto* btn = new QPushButton(QStringLiteral("新建窗口"), root);
		btn->setFixedSize(140, 36);
		lay->addStretch();
		lay->addWidget(label, 0, Qt::AlignCenter);
		lay->addSpacing(12);
		lay->addWidget(btn, 0, Qt::AlignCenter);
		lay->addStretch();
		connect(btn, &QPushButton::clicked, this, &PageWindow::requestNewWindow);
	}

	ClientApp::ClientApp(QString endpoint, QString token, QObject* parent)
		: QObject(parent)
		, endpoint_(std::move(endpoint))
		, token_(std::move(token))
	{
	}

	bool ClientApp::connectToHost()
	{
		socket_ = new QLocalSocket(this);
		socket_->connectToServer(endpoint_);
		if (!socket_->waitForConnected(5000))
		{
			qWarning("connect failed: %s", qPrintable(socket_->errorString()));
			return false;
		}
		channel_ = std::make_unique<mps::ipc::EnvelopeChannel>(socket_, this);
		channel_->setHandler(
			[this](shell::ipc::v1::Envelope env)
			{
				onEnvelope(std::move(env));
			});
		connect(channel_.get(), &mps::ipc::EnvelopeChannel::disconnected, qApp, &QCoreApplication::quit);
		sendHello();
		return true;
	}

	void ClientApp::sendHello()
	{
		shell::ipc::v1::Envelope env;
		env.set_protocol(1);
		env.set_id(mps::ipc::newCorrelationId());
		env.set_dir(shell::ipc::v1::DIR_EVT);
		env.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
		auto* hello = env.mutable_hello();
		hello->set_min_protocol(1);
		hello->set_max_protocol(1);
#ifdef Q_OS_WIN
		hello->set_pid(static_cast<uint32_t>(GetCurrentProcessId()));
#else
		hello->set_pid(static_cast<uint32_t>(QCoreApplication::applicationPid()));
#endif
		hello->set_app_name("demo_client");
		auto* caps = hello->mutable_caps();
		caps->set_embed(shell::ipc::v1::EMBED_HWND);
		caps->set_tab_drag(true);
		caps->set_heartbeat(true);
		caps->set_invoke(true);
		caps->set_multi_sub_window(true);
		channel_->send(env);
	}

	void ClientApp::ensureMainReported()
	{
		if (mainReported_ || pages_.isEmpty())
		{
			return;
		}
		auto* first = pages_.begin().value();
		first->winId();
		shell::ipc::v1::Envelope env;
		env.set_protocol(1);
		env.set_id(mps::ipc::newCorrelationId());
		env.set_dir(shell::ipc::v1::DIR_EVT);
		env.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
		auto* added = env.mutable_main_window_added();
		added->set_wid(static_cast<uint64_t>(first->winId()));
#ifdef Q_OS_WIN
		added->set_pid(static_cast<uint32_t>(GetCurrentProcessId()));
#else
		added->set_pid(static_cast<uint32_t>(QCoreApplication::applicationPid()));
#endif
		added->set_visible(true);
		channel_->send(env);
		mainReported_ = true;
	}

	void ClientApp::createPage(qint64 tabId, const QString& title)
	{
		auto* page = new PageWindow(tabId, title);
		pages_.insert(tabId, page);
		connect(page, &PageWindow::requestNewWindow, this,
				[this, tabId]
				{
					shell::ipc::v1::Envelope env;
					env.set_protocol(1);
					env.set_id(mps::ipc::newCorrelationId());
					env.set_dir(shell::ipc::v1::DIR_REQ);
					env.set_tab_id(tabId);
					env.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
					env.mutable_invoke()->set_method("demo.request_new_window");
					channel_->send(env);
				});
		page->show();
		page->winId();

		if (!mainReported_)
		{
			ensureMainReported();
		}

		shell::ipc::v1::Envelope env;
		env.set_protocol(1);
		env.set_id(mps::ipc::newCorrelationId());
		env.set_dir(shell::ipc::v1::DIR_EVT);
		env.set_tab_id(tabId);
		env.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
		auto* added = env.mutable_sub_window_added();
		added->set_title(title.toStdString());
		added->set_wid(static_cast<uint64_t>(page->winId()));
		channel_->send(env);

		activatePage(tabId);
	}

	void ClientApp::closePage(qint64 tabId)
	{
		auto* page = pages_.take(tabId);
		if (!page)
		{
			return;
		}
		if (active_ == page)
		{
			active_ = nullptr;
		}
		page->hide();
		page->deleteLater();
		shell::ipc::v1::Envelope env;
		env.set_protocol(1);
		env.set_id(mps::ipc::newCorrelationId());
		env.set_dir(shell::ipc::v1::DIR_EVT);
		env.set_tab_id(tabId);
		env.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
		env.mutable_sub_window_removed();
		channel_->send(env);
	}

	void ClientApp::activatePage(qint64 tabId)
	{
		auto* page = pages_.value(tabId, nullptr);
		if (!page)
		{
			return;
		}
		// Do NOT hide other pages: each may be SetParent'd into a different Host shell.
		// Visibility of non-active embeds is owned by the Host (ShowWindow / clearForeignWindow).
		page->show();
		active_ = page;
	}

	void ClientApp::onEnvelope(shell::ipc::v1::Envelope env)
	{
		if (env.has_hello_ack())
		{
			return;
		}
		if (env.has_create_sub_window())
		{
			createPage(env.tab_id(), QString::fromStdString(env.create_sub_window().title()));
			return;
		}
		if (env.has_active_sub_window())
		{
			activatePage(env.tab_id());
			return;
		}
		if (env.has_query_close_sub_window())
		{
			shell::ipc::v1::Envelope res;
			res.set_protocol(1);
			res.set_id(env.id());
			res.set_dir(shell::ipc::v1::DIR_RES);
			res.set_tab_id(env.tab_id());
			res.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
			res.mutable_query_close_sub_window_result()->set_accept(true);
			channel_->send(res);
			closePage(env.tab_id());
			return;
		}
		if (env.has_set_drag_suppress() || env.has_notify_main_window_reattachment())
		{
			return;
		}
		if (env.has_invoke())
		{
			shell::ipc::v1::Envelope res;
			res.set_protocol(1);
			res.set_id(env.id());
			res.set_dir(shell::ipc::v1::DIR_RES);
			res.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
			auto* err = res.mutable_error();
			err->set_code(shell::ipc::v1::ERROR_UNIMPLEMENTED);
			err->set_message("unimplemented");
			channel_->send(res);
			return;
		}
		if (env.has_ping())
		{
			shell::ipc::v1::Envelope res;
			res.set_protocol(1);
			res.set_id(env.id());
			res.set_dir(shell::ipc::v1::DIR_RES);
			res.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
			res.mutable_pong();
			channel_->send(res);
		}
	}
} // namespace mps::client
