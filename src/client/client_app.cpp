#include "client_app.hpp"

#include "envelope_builder.hpp"
#include "heartbeat_policy.hpp"
#include "theme_scheme.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QLocalSocket>
#include <QScreen>
#include <QThread>
#include <QTimer>
#include <QWidget>
#include <QWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace mps::client
{
	ClientApp::ClientApp(QString endpoint, QString token, ContentViewFactory factory, bool enableHeartbeat, QObject* parent)
		: QObject(parent)
		, m_endpoint(std::move(endpoint))
		, m_token(std::move(token))
		, m_factory(std::move(factory))
		, m_enableHeartbeat(enableHeartbeat)
	{
	}

	ClientApp::~ClientApp()
	{
		for (auto it = m_views.begin(); it != m_views.end(); ++it)
		{
			delete it.value();
		}
		m_views.clear();
		m_active = nullptr;
	}

	void ClientApp::applyThemeScheme(mps::theme::Scheme scheme)
	{
		if (scheme != mps::theme::Scheme::Light && scheme != mps::theme::Scheme::Dark)
		{
			scheme = mps::theme::Scheme::Light;
		}
		if (m_appearanceHandler)
		{
			m_appearanceHandler(scheme);
		}
		for (auto it = m_views.begin(); it != m_views.end(); ++it)
		{
			if (ContentView* view = it.value())
			{
				view->applyTheme(scheme);
			}
		}
	}

	void ClientApp::requestThemeFromHost(mps::theme::Scheme scheme, qint64 tabId)
	{
		if (scheme != mps::theme::Scheme::Light && scheme != mps::theme::Scheme::Dark)
		{
			scheme = mps::theme::Scheme::Light;
		}
		// Optimistic local apply; Host broadcast / InvokeResult remains the SSOT.
		applyThemeScheme(scheme);
		if (!m_channel)
		{
			return;
		}
		const QByteArray params = mps::theme::toParams(scheme);
		auto env =
			mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_REQ, QDateTime::currentMSecsSinceEpoch(), 0, tabId);
		env->mutable_invoke()->set_method("theme.set");
		env->mutable_invoke()->set_params(params.constData(), static_cast<int>(params.size()));
		m_channel->send(env);
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
			[this](mps::ipc::EnvelopePtr env)
			{
				onEnvelope(std::move(env));
			});
		connect(m_channel.get(), &mps::ipc::EnvelopeChannel::disconnected, this,
				[this]
				{
					stopHeartbeatTimer();
					qApp->quit();
				});
		// Give Host a moment to attach the socket and set the Envelope handler.
		QThread::msleep(150);
		if (!m_socket || m_socket->state() != QLocalSocket::ConnectedState)
		{
			qWarning("disconnected before Hello");
			return false;
		}
		sendHello();
		m_socket->flush();
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
		env->mutable_heartbeat();
		m_channel->send(env);
	}

	void ClientApp::sendHello()
	{
		auto env = mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_EVT, QDateTime::currentMSecsSinceEpoch());
		auto* hello = env->mutable_hello();
		hello->set_min_protocol(1);
		hello->set_max_protocol(1);
#ifdef Q_OS_WIN
		hello->set_pid(static_cast<uint32_t>(GetCurrentProcessId()));
#else
		hello->set_pid(static_cast<uint32_t>(QCoreApplication::applicationPid()));
#endif
		hello->set_app_name(m_appName.toStdString());
		auto* caps = hello->mutable_caps();
		caps->set_embed(shell::ipc::v1::EMBED_HWND);
		caps->set_tab_drag(true);
		caps->set_heartbeat(m_enableHeartbeat);
		caps->set_invoke(true);
		caps->set_multi_tab(true);
		m_channel->send(env);
	}

	void ClientApp::ensureMainWindowAddedSent()
	{
		if (m_mainWindowAddedSent || m_views.isEmpty())
		{
			return;
		}
		ContentView* first = m_views.begin().value();
		QWidget* w = first ? first->widget() : nullptr;
		if (!w)
		{
			return;
		}
		w->winId();
		auto env = mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_EVT, QDateTime::currentMSecsSinceEpoch());
		auto* added = env->mutable_main_window_added();
		added->set_wid(static_cast<uint64_t>(w->winId()));
#ifdef Q_OS_WIN
		added->set_pid(static_cast<uint32_t>(GetCurrentProcessId()));
#else
		added->set_pid(static_cast<uint32_t>(QCoreApplication::applicationPid()));
#endif
		added->set_visible(true);
		m_channel->send(env);
		m_mainWindowAddedSent = true;
	}

	void ClientApp::createView(qint64 tabId, const QString& title)
	{
		if (!m_factory)
		{
			qWarning("ClientApp: ContentViewFactory is empty");
			return;
		}
#ifdef Q_OS_WIN
		// Sibling views may be created after an earlier HWND was SetParent'd into Host;
		// re-assert PMV2 so the new top-level HWND is not born under a degraded thread context.
		SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif
		std::unique_ptr<ContentView> owned = m_factory(tabId, title);
		if (!owned)
		{
			qWarning("ClientApp: ContentViewFactory returned null for tabId=%lld", static_cast<long long>(tabId));
			return;
		}
		ContentView* view = owned.get();
		view->onRequestNewContentView = [this, tabId]()
		{
			auto env = mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_REQ, QDateTime::currentMSecsSinceEpoch(),
											  0, tabId);
			env->mutable_invoke()->set_method(m_requestNewContentViewMethod.toStdString());
			m_channel->send(env);
		};
		view->onRequestTheme = [this, tabId](mps::theme::Scheme scheme)
		{
			requestThemeFromHost(scheme, tabId);
		};
		view->onRequestTabTitle = [this, tabId](const QString& title)
		{
			if (!m_channel)
			{
				return;
			}
			auto env = mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_REQ, QDateTime::currentMSecsSinceEpoch(),
											  0, tabId);
			env->mutable_invoke()->set_method("shell.set_tab_title");
			const QByteArray params = title.toUtf8();
			env->mutable_invoke()->set_params(params.constData(), static_cast<int>(params.size()));
			m_channel->send(env);
		};

		QWidget* w = view->widget();
		if (!w)
		{
			qWarning("ClientApp: content view widget is null for tabId=%lld", static_cast<long long>(tabId));
			return;
		}

		m_views.insert(tabId, owned.release());

		w->createWinId();
		if (QWindow* wh = w->windowHandle())
		{
			if (QScreen* screen = QGuiApplication::primaryScreen())
			{
				wh->setScreen(screen);
			}
		}
		view->realizeChrome();
#ifdef Q_OS_WIN
		// Defer first on-screen paint until Host SetParent + SetWindowPos (see syncAfterEmbed).
		w->setAttribute(Qt::WA_DontShowOnScreen, true);
#endif
		w->show();
		w->winId();

		if (!m_mainWindowAddedSent)
		{
			ensureMainWindowAddedSent();
		}

		auto env =
			mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_EVT, QDateTime::currentMSecsSinceEpoch(), 0, tabId);
		auto* added = env->mutable_sub_window_added();
		added->set_title(title.toStdString());
		added->set_wid(static_cast<uint64_t>(w->winId()));
		m_channel->send(env);

		activateView(tabId);
#ifdef Q_OS_WIN
		// Fallback if WM_SIZE was missed; syncAfterEmbed no-ops until GetParent is set.
		QTimer::singleShot(50, this,
						   [this, tabId]
						   {
							   if (ContentView* p = m_views.value(tabId, nullptr))
							   {
								   p->syncAfterEmbed();
							   }
						   });
#endif
	}

	void ClientApp::closeView(qint64 tabId)
	{
		ContentView* view = m_views.take(tabId);
		if (!view)
		{
			return;
		}
		if (m_active == view)
		{
			m_active = nullptr;
		}
		if (QWidget* w = view->widget())
		{
			w->hide();
		}
		delete view;
		auto env =
			mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_EVT, QDateTime::currentMSecsSinceEpoch(), 0, tabId);
		env->mutable_sub_window_removed();
		m_channel->send(env);
	}

	void ClientApp::activateView(qint64 tabId)
	{
		ContentView* view = m_views.value(tabId, nullptr);
		if (!view)
		{
			return;
		}
		// Do NOT hide other views: each may be SetParent'd into a different Host shell.
		// Visibility of non-active embeds is owned by the Host (ShowWindow / clearClientWindow).
		if (QWidget* w = view->widget())
		{
			w->show();
		}
		m_active = view;
		view->activate();
	}

	void ClientApp::onEnvelope(mps::ipc::EnvelopePtr env)
	{
		if (!env)
		{
			return;
		}
		if (env->has_hello_ack())
		{
			if (m_enableHeartbeat && env->hello_ack().host_caps().heartbeat())
			{
				startHeartbeatTimer();
			}
			return;
		}
		if (env->has_create_sub_window())
		{
			createView(env->tab_id(), QString::fromStdString(env->create_sub_window().title()));
			return;
		}
		if (env->has_active_sub_window())
		{
			activateView(env->tab_id());
			return;
		}
		if (env->has_query_close_sub_window())
		{
			auto res = mps::ipc::makeResponse(1, env->id(), QDateTime::currentMSecsSinceEpoch(), env->tab_id());
			res->mutable_query_close_sub_window_result()->set_accept(true);
			m_channel->send(res);
			closeView(env->tab_id());
			return;
		}
		if (env->has_set_drag_suppress() || env->has_notify_main_window_reattachment())
		{
			// Demo Client intentionally ignores these EVTs (Host still emits for contract).
			// See docs/zh/m5-gap-audit.md G2/G3 — accepted Demo limit.
			return;
		}
		if (env->has_invoke())
		{
			if (env->invoke().method() == "theme.set")
			{
				const QByteArray params = QByteArray::fromStdString(env->invoke().params());
				mps::theme::Scheme wire = mps::theme::Scheme::Light;
				if (!mps::theme::fromParams(params, &wire))
				{
					auto res = mps::ipc::makeResponse(1, env->id(), QDateTime::currentMSecsSinceEpoch());
					auto* err = res->mutable_error();
					err->set_code(shell::ipc::v1::ERROR_PROTOCOL);
					err->set_message("theme.set params must be light or dark");
					m_channel->send(res);
					return;
				}
				applyThemeScheme(wire);
				const QByteArray wireBytes = mps::theme::toParams(wire);
				auto res = mps::ipc::makeResponse(1, env->id(), QDateTime::currentMSecsSinceEpoch());
				res->mutable_invoke_result()->set_payload(wireBytes.constData(), static_cast<int>(wireBytes.size()));
				m_channel->send(res);
				return;
			}
			if (m_invokeHandler)
			{
				ContentView* view = m_views.value(env->tab_id(), nullptr);
				if (!view)
				{
					view = m_active;
				}
				const QString method = QString::fromStdString(env->invoke().method());
				const QByteArray params = QByteArray::fromStdString(env->invoke().params());
				QByteArray payload;
				QString error;
				if (m_invokeHandler(view, method, params, &payload, &error))
				{
					if (!error.isEmpty())
					{
						auto res = mps::ipc::makeResponse(1, env->id(), QDateTime::currentMSecsSinceEpoch(), env->tab_id());
						auto* err = res->mutable_error();
						err->set_code(shell::ipc::v1::ERROR_PROTOCOL);
						err->set_message(error.toStdString());
						m_channel->send(res);
						return;
					}
					auto res = mps::ipc::makeResponse(1, env->id(), QDateTime::currentMSecsSinceEpoch(), env->tab_id());
					res->mutable_invoke_result()->set_payload(payload.constData(), static_cast<int>(payload.size()));
					m_channel->send(res);
					return;
				}
			}
			auto res = mps::ipc::makeResponse(1, env->id(), QDateTime::currentMSecsSinceEpoch());
			auto* err = res->mutable_error();
			err->set_code(shell::ipc::v1::ERROR_UNIMPLEMENTED);
			err->set_message("unimplemented");
			m_channel->send(res);
			return;
		}
		if (env->has_ping())
		{
			auto res = mps::ipc::makeResponse(1, env->id(), QDateTime::currentMSecsSinceEpoch());
			res->mutable_pong();
			m_channel->send(res);
		}
	}
} // namespace mps::client
