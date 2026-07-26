#include "client_session.hpp"

#include "envelope_builder.hpp"
#include "heartbeat_policy.hpp"
#include "theme_scheme.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <QLocalSocket>
#include <QTimer>

namespace mps::host
{
	namespace
	{
		qint64 g_nextPageId = 1;
	}

	ClientSession::ClientSession(int clientIndex, QString endpoint, QString requestNewWindowMethod, QObject* parent)
		: QObject(parent)
		, m_clientIndex(clientIndex)
		, m_pageId(g_nextPageId++)
		, m_endpoint(std::move(endpoint))
		, m_requestNewWindowMethod(std::move(requestNewWindowMethod))
	{
		if (m_requestNewWindowMethod.isEmpty())
		{
			m_requestNewWindowMethod = QStringLiteral("shell.request_new_window");
		}
	}

	ClientSession::~ClientSession()
	{
		stopHeartbeatWatch();
		if (m_process)
		{
			m_process->disconnect(this);
			m_process->kill();
			m_process->waitForFinished(1000);
		}
		if (m_socket)
		{
			m_socket->disconnect(this);
			m_socket->abort();
		}
	}

	void ClientSession::startClientProcess(const QString& clientExe, const QString& token)
	{
		m_process = new QProcess(this);
		connect(m_process, &QProcess::finished, this,
				[this](int, QProcess::ExitStatus)
				{
					markDead();
				});
		connect(m_process, &QProcess::errorOccurred, this,
				[this](QProcess::ProcessError err)
				{
					qWarning("Client process error %d: %s (exe=%s)", static_cast<int>(err), qPrintable(m_process->errorString()),
							 qPrintable(m_process->program()));
					if (err == QProcess::FailedToStart)
					{
						markDead();
					}
				});
		QStringList args;
		args << QStringLiteral("--from-host") << QStringLiteral("--endpoint=%1").arg(m_endpoint)
			 << QStringLiteral("--pipe-token=%1").arg(token) << QStringLiteral("--protocol=1");
		if (qEnvironmentVariableIsSet("MPS_CLIENT_NO_HEARTBEAT"))
		{
			args << QStringLiteral("--no-heartbeat");
		}
		const QFileInfo fi(clientExe);
		if (fi.exists())
		{
			m_process->setWorkingDirectory(fi.absolutePath());
		}
		m_process->setProgram(clientExe);
		m_process->setArguments(args);
		// Async start only: waitForStarted() on the GUI thread blocks the event loop so the
		// started notification never arrives (Create Client appears to do nothing).
		m_process->start();
	}

	void ClientSession::attachSocket(QLocalSocket* socket)
	{
		m_socket = socket;
		m_socket->setParent(this);
		m_channel = std::make_unique<mps::ipc::EnvelopeChannel>(m_socket, this);
		m_channel->setHandler(
			[this](mps::ipc::EnvelopePtr env)
			{
				onEnvelope(std::move(env));
			});
		connect(m_channel.get(), &mps::ipc::EnvelopeChannel::disconnected, this,
				[this]
				{
					markDead();
				});
	}

	void ClientSession::markDead()
	{
		if (m_dead)
		{
			return;
		}
		m_dead = true;
		stopHeartbeatWatch();
		emit sessionDead(this);
	}

	void ClientSession::terminateProcess()
	{
		if (m_dead)
		{
			return;
		}
		if (m_process)
		{
			m_process->kill();
		}
		else
		{
			markDead();
		}
	}

	void ClientSession::noteHeartbeat()
	{
		m_lastHeartbeatMs = QDateTime::currentMSecsSinceEpoch();
		if (m_unhealthy)
		{
			m_unhealthy = false;
			emit sessionHealthy(this);
		}
	}

	void ClientSession::startHeartbeatWatch()
	{
		if (!m_heartbeatNegotiated || m_dead)
		{
			return;
		}
		if (!m_heartbeatWatch)
		{
			m_heartbeatWatch = new QTimer(this);
			m_heartbeatWatch->setInterval(static_cast<int>(mps::ipc::kHeartbeatWatchTickMs));
			connect(m_heartbeatWatch, &QTimer::timeout, this, &ClientSession::onHeartbeatWatchTick);
		}
		m_lastHeartbeatMs = QDateTime::currentMSecsSinceEpoch();
		m_heartbeatWatch->start();
	}

	void ClientSession::stopHeartbeatWatch()
	{
		if (m_heartbeatWatch)
		{
			m_heartbeatWatch->stop();
		}
	}

	void ClientSession::onHeartbeatWatchTick()
	{
		if (m_dead || !m_heartbeatNegotiated || m_unhealthy)
		{
			return;
		}
		const qint64 now = QDateTime::currentMSecsSinceEpoch();
		if (mps::ipc::isHeartbeatTimedOut(m_lastHeartbeatMs, now))
		{
			m_unhealthy = true;
			emit sessionUnhealthy(this);
		}
	}

	void ClientSession::sendHelloAck()
	{
		auto env = mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_EVT, QDateTime::currentMSecsSinceEpoch());
		auto* ack = env->mutable_hello_ack();
		ack->set_protocol(1);
		ack->set_session_id(QString::number(m_pageId).toStdString());
		auto* caps = ack->mutable_host_caps();
		caps->set_embed(shell::ipc::v1::EMBED_HWND);
		caps->set_tab_drag(true);
		caps->set_heartbeat(true);
		caps->set_invoke(true);
		caps->set_multi_sub_window(true);
		m_channel->send(env);
		if (m_heartbeatNegotiated)
		{
			startHeartbeatWatch();
		}
	}

	void ClientSession::requestCreateSubWindow(qint64 tabId, const QString& title)
	{
		if (m_dead || !m_channel)
		{
			return;
		}
		m_pendingTabs.push_back(tabId);
		auto env = mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_REQ, QDateTime::currentMSecsSinceEpoch(),
										  m_pageId, tabId);
		env->mutable_create_sub_window()->set_title(title.toStdString());
		m_channel->send(env);
	}

	void ClientSession::requestActivate(qint64 tabId)
	{
		if (m_dead || !m_channel)
		{
			return;
		}
		auto env = mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_EVT, QDateTime::currentMSecsSinceEpoch(),
										  m_pageId, tabId);
		env->mutable_active_sub_window();
		m_channel->send(env);
	}

	void ClientSession::requestClose(qint64 tabId)
	{
		if (m_dead || !m_channel)
		{
			return;
		}
		auto env = mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_REQ, QDateTime::currentMSecsSinceEpoch(),
										  m_pageId, tabId);
		env->mutable_query_close_sub_window();
		m_channel->send(env);
	}

	void ClientSession::notifyReattachment(qint64 shellId)
	{
		if (m_dead || !m_channel)
		{
			return;
		}
		auto env =
			mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_EVT, QDateTime::currentMSecsSinceEpoch(), m_pageId);
		env->mutable_notify_main_window_reattachment()->set_shell_id(shellId);
		m_channel->send(env);
	}

	void ClientSession::setDragSuppress(bool on)
	{
		if (m_dead || !m_channel)
		{
			return;
		}
		auto env =
			mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_EVT, QDateTime::currentMSecsSinceEpoch(), m_pageId);
		env->mutable_set_drag_suppress()->set_suppress(on);
		m_channel->send(env);
	}

	void ClientSession::pushThemeScheme(const QByteArray& params)
	{
		if (m_dead || !m_channel)
		{
			return;
		}
		auto env =
			mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_REQ, QDateTime::currentMSecsSinceEpoch(), m_pageId);
		env->mutable_invoke()->set_method("theme.set");
		env->mutable_invoke()->set_params(params.constData(), static_cast<int>(params.size()));
		m_channel->send(env);
	}

	void ClientSession::onEnvelope(mps::ipc::EnvelopePtr env)
	{
		if (!env)
		{
			return;
		}
		if (env->has_hello() && !m_helloSeen)
		{
			m_helloSeen = true;
			m_heartbeatNegotiated = env->hello().caps().heartbeat();
			sendHelloAck();
			emit sessionHelloOk(this);
			return;
		}
		if (env->has_main_window_added())
		{
			m_mainWid = static_cast<quintptr>(env->main_window_added().wid());
			m_ready = true;
			emit sessionReady(this);
			return;
		}
		if (env->has_sub_window_added())
		{
			qint64 tabId = env->tab_id();
			if (tabId == 0 && !m_pendingTabs.isEmpty())
			{
				tabId = m_pendingTabs.takeFirst();
			}
			else if (!m_pendingTabs.isEmpty() && m_pendingTabs.front() == tabId)
			{
				m_pendingTabs.pop_front();
			}
			quintptr wid = static_cast<quintptr>(env->sub_window_added().wid());
			if (wid == 0)
			{
				wid = m_mainWid;
			}
			const QString title = QString::fromStdString(env->sub_window_added().title());
			emit subWindowAdded(this, tabId, title, wid);
			return;
		}
		if (env->has_sub_window_removed())
		{
			const qint64 tabId = env->tab_id();
			emit subWindowRemoved(this, tabId);
			return;
		}
		if (env->has_query_close_sub_window_result())
		{
			// Accept → tear down Host tab immediately; SubWindowRemoved is idempotent backup.
			if (env->query_close_sub_window_result().accept())
			{
				const qint64 tabId = env->tab_id();
				emit subWindowRemoved(this, tabId);
			}
			return;
		}
		if (env->has_invoke())
		{
			// Client asks Host to create another window in this session.
			if (QString::fromStdString(env->invoke().method()) == m_requestNewWindowMethod)
			{
				emit invokeNewWindow(this, env->tab_id());
				auto res = mps::ipc::makeResponse(1, env->id(), QDateTime::currentMSecsSinceEpoch());
				res->mutable_invoke_result()->set_payload("ok");
				m_channel->send(res);
				return;
			}
			if (env->invoke().method() == "theme.set")
			{
				const QByteArray params = QByteArray::fromStdString(env->invoke().params());
				mps::theme::Scheme scheme = mps::theme::Scheme::Light;
				if (!mps::theme::fromParams(params, &scheme))
				{
					auto res = mps::ipc::makeResponse(1, env->id(), QDateTime::currentMSecsSinceEpoch());
					auto* err = res->mutable_error();
					err->set_code(shell::ipc::v1::ERROR_PROTOCOL);
					err->set_message("theme.set params must be light or dark");
					m_channel->send(res);
					return;
				}
				emit themeSetRequested(this, scheme);
				const QByteArray wire = mps::theme::toParams(scheme);
				auto res = mps::ipc::makeResponse(1, env->id(), QDateTime::currentMSecsSinceEpoch());
				res->mutable_invoke_result()->set_payload(wire.constData(), static_cast<int>(wire.size()));
				m_channel->send(res);
				return;
			}
			auto res = mps::ipc::makeResponse(1, env->id(), QDateTime::currentMSecsSinceEpoch());
			auto* err = res->mutable_error();
			err->set_code(shell::ipc::v1::ERROR_UNIMPLEMENTED);
			err->set_message("Invoke method not implemented");
			m_channel->send(res);
			return;
		}
		if (env->has_heartbeat())
		{
			if (m_heartbeatNegotiated)
			{
				noteHeartbeat();
			}
			return;
		}
	}
} // namespace mps::host
