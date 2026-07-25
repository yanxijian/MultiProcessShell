#include "client_app.hpp"

#include "envelope_builder.hpp"
#include "heartbeat_policy.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace mps::client
{
	PageWindow::PageWindow(qint64 tabId, QString title, QWidget* parent)
		: QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
		, m_tabId(tabId)
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

	ClientApp::ClientApp(QString endpoint, QString token, bool enableHeartbeat, QObject* parent)
		: QObject(parent)
		, m_endpoint(std::move(endpoint))
		, m_token(std::move(token))
		, m_enableHeartbeat(enableHeartbeat)
	{
	}

	bool ClientApp::connectToHost()
	{
		m_socket = new QLocalSocket(this);
		m_socket->connectToServer(m_endpoint);
		if (!m_socket->waitForConnected(5000))
		{
			qWarning("connect failed: %s", qPrintable(m_socket->errorString()));
			return false;
		}
		m_channel = std::make_unique<mps::ipc::EnvelopeChannel>(m_socket, this);
		m_channel->setHandler(
			[this](shell::ipc::v1::Envelope env)
			{
				onEnvelope(std::move(env));
			});
		connect(m_channel.get(), &mps::ipc::EnvelopeChannel::disconnected, this,
				[this]
				{
					stopHeartbeatTimer();
					qApp->quit();
				});
		sendHello();
		return true;
	}

	void ClientApp::startHeartbeatTimer()
	{
		if (!m_enableHeartbeat || m_heartbeatArmed || !m_channel)
		{
			return;
		}
		m_heartbeatArmed = true;
		if (!m_heartbeatTimer)
		{
			m_heartbeatTimer = new QTimer(this);
			m_heartbeatTimer->setInterval(static_cast<int>(mps::ipc::kHeartbeatIntervalMs));
			connect(m_heartbeatTimer, &QTimer::timeout, this, &ClientApp::sendHeartbeat);
		}
		sendHeartbeat();
		m_heartbeatTimer->start();
	}

	void ClientApp::stopHeartbeatTimer()
	{
		if (m_heartbeatTimer)
		{
			m_heartbeatTimer->stop();
		}
		m_heartbeatArmed = false;
	}

	void ClientApp::sendHeartbeat()
	{
		if (!m_channel)
		{
			return;
		}
		auto env = mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_EVT, QDateTime::currentMSecsSinceEpoch());
		env.mutable_heartbeat();
		m_channel->send(env);
	}

	void ClientApp::sendHello()
	{
		auto env = mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_EVT, QDateTime::currentMSecsSinceEpoch());
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
		caps->set_heartbeat(m_enableHeartbeat);
		caps->set_invoke(true);
		caps->set_multi_sub_window(true);
		m_channel->send(env);
	}

	void ClientApp::ensureMainReported()
	{
		if (m_mainReported || m_pages.isEmpty())
		{
			return;
		}
		auto* first = m_pages.begin().value();
		first->winId();
		auto env = mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_EVT, QDateTime::currentMSecsSinceEpoch());
		auto* added = env.mutable_main_window_added();
		added->set_wid(static_cast<uint64_t>(first->winId()));
#ifdef Q_OS_WIN
		added->set_pid(static_cast<uint32_t>(GetCurrentProcessId()));
#else
		added->set_pid(static_cast<uint32_t>(QCoreApplication::applicationPid()));
#endif
		added->set_visible(true);
		m_channel->send(env);
		m_mainReported = true;
	}

	void ClientApp::createPage(qint64 tabId, const QString& title)
	{
		auto* page = new PageWindow(tabId, title);
		m_pages.insert(tabId, page);
		connect(page, &PageWindow::requestNewWindow, this,
				[this, tabId]
				{
					auto env = mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_REQ,
													  QDateTime::currentMSecsSinceEpoch(), 0, tabId);
					env.mutable_invoke()->set_method("demo.request_new_window");
					m_channel->send(env);
				});
		page->show();
		page->winId();

		if (!m_mainReported)
		{
			ensureMainReported();
		}

		auto env =
			mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_EVT, QDateTime::currentMSecsSinceEpoch(), 0, tabId);
		auto* added = env.mutable_sub_window_added();
		added->set_title(title.toStdString());
		added->set_wid(static_cast<uint64_t>(page->winId()));
		m_channel->send(env);

		activatePage(tabId);
	}

	void ClientApp::closePage(qint64 tabId)
	{
		auto* page = m_pages.take(tabId);
		if (!page)
		{
			return;
		}
		if (m_active == page)
		{
			m_active = nullptr;
		}
		page->hide();
		page->deleteLater();
		auto env =
			mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_EVT, QDateTime::currentMSecsSinceEpoch(), 0, tabId);
		env.mutable_sub_window_removed();
		m_channel->send(env);
	}

	void ClientApp::activatePage(qint64 tabId)
	{
		auto* page = m_pages.value(tabId, nullptr);
		if (!page)
		{
			return;
		}
		// Do NOT hide other pages: each may be SetParent'd into a different Host shell.
		// Visibility of non-active embeds is owned by the Host (ShowWindow / clearForeignWindow).
		page->show();
		m_active = page;
	}

	void ClientApp::onEnvelope(shell::ipc::v1::Envelope env)
	{
		if (env.has_hello_ack())
		{
			if (m_enableHeartbeat && env.hello_ack().host_caps().heartbeat())
			{
				startHeartbeatTimer();
			}
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
			auto res = mps::ipc::makeResponse(1, env.id(), QDateTime::currentMSecsSinceEpoch(), env.tab_id());
			res.mutable_query_close_sub_window_result()->set_accept(true);
			m_channel->send(res);
			closePage(env.tab_id());
			return;
		}
		if (env.has_set_drag_suppress() || env.has_notify_main_window_reattachment())
		{
			// Demo Client intentionally ignores these EVTs (Host still emits for contract).
			// See docs/zh/m5-gap-audit.md G2/G3 — accepted Demo limit.
			return;
		}
		if (env.has_invoke())
		{
			auto res = mps::ipc::makeResponse(1, env.id(), QDateTime::currentMSecsSinceEpoch());
			auto* err = res.mutable_error();
			err->set_code(shell::ipc::v1::ERROR_UNIMPLEMENTED);
			err->set_message("unimplemented");
			m_channel->send(res);
			return;
		}
		if (env.has_ping())
		{
			auto res = mps::ipc::makeResponse(1, env.id(), QDateTime::currentMSecsSinceEpoch());
			res.mutable_pong();
			m_channel->send(res);
		}
	}
} // namespace mps::client
