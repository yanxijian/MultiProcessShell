#include "client_session.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QHash>
#include <QLocalSocket>

namespace mps::host
{
	namespace
	{
		qint64 g_nextPageId = 1;
	}

	ClientSession::ClientSession(int clientIndex, QString endpoint, QObject* parent)
		: QObject(parent)
		, m_clientIndex(clientIndex)
		, m_pageId(g_nextPageId++)
		, m_endpoint(std::move(endpoint))
	{
	}

	ClientSession::~ClientSession()
	{
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
		QStringList args;
		args << QStringLiteral("--from-host") << QStringLiteral("--endpoint=%1").arg(m_endpoint)
			 << QStringLiteral("--pipe-token=%1").arg(token) << QStringLiteral("--protocol=1");
		m_process->start(clientExe, args);
	}

	void ClientSession::attachSocket(QLocalSocket* socket)
	{
		m_socket = socket;
		m_socket->setParent(this);
		m_channel = std::make_unique<mps::ipc::EnvelopeChannel>(m_socket, this);
		m_channel->setHandler(
			[this](shell::ipc::v1::Envelope env)
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
		emit sessionDead(this);
	}

	void ClientSession::sendHelloAck()
	{
		shell::ipc::v1::Envelope env;
		env.set_protocol(1);
		env.set_id(mps::ipc::newCorrelationId());
		env.set_dir(shell::ipc::v1::DIR_EVT);
		env.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
		auto* ack = env.mutable_hello_ack();
		ack->set_protocol(1);
		ack->set_session_id(QString::number(m_pageId).toStdString());
		auto* caps = ack->mutable_host_caps();
		caps->set_embed(shell::ipc::v1::EMBED_HWND);
		caps->set_tab_drag(true);
		caps->set_heartbeat(true);
		caps->set_invoke(true);
		caps->set_multi_sub_window(true);
		m_channel->send(env);
	}

	void ClientSession::requestCreateSubWindow(qint64 tabId, const QString& title)
	{
		if (m_dead || !m_channel)
		{
			return;
		}
		m_pendingTabs.push_back(tabId);
		shell::ipc::v1::Envelope env;
		env.set_protocol(1);
		env.set_id(mps::ipc::newCorrelationId());
		env.set_dir(shell::ipc::v1::DIR_REQ);
		env.set_page_id(m_pageId);
		env.set_tab_id(tabId);
		env.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
		env.mutable_create_sub_window()->set_title(title.toStdString());
		m_channel->send(env);
	}

	void ClientSession::requestActivate(qint64 tabId)
	{
		if (m_dead || !m_channel)
		{
			return;
		}
		shell::ipc::v1::Envelope env;
		env.set_protocol(1);
		env.set_id(mps::ipc::newCorrelationId());
		env.set_dir(shell::ipc::v1::DIR_EVT);
		env.set_page_id(m_pageId);
		env.set_tab_id(tabId);
		env.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
		env.mutable_active_sub_window();
		m_channel->send(env);
	}

	void ClientSession::requestClose(qint64 tabId)
	{
		if (m_dead || !m_channel)
		{
			return;
		}
		shell::ipc::v1::Envelope env;
		env.set_protocol(1);
		env.set_id(mps::ipc::newCorrelationId());
		env.set_dir(shell::ipc::v1::DIR_REQ);
		env.set_page_id(m_pageId);
		env.set_tab_id(tabId);
		env.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
		env.mutable_query_close_sub_window();
		m_channel->send(env);
	}

	void ClientSession::notifyReattachment(qint64 shellId)
	{
		if (m_dead || !m_channel)
		{
			return;
		}
		shell::ipc::v1::Envelope env;
		env.set_protocol(1);
		env.set_id(mps::ipc::newCorrelationId());
		env.set_dir(shell::ipc::v1::DIR_EVT);
		env.set_page_id(m_pageId);
		env.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
		env.mutable_notify_main_window_reattachment()->set_shell_id(shellId);
		m_channel->send(env);
	}

	void ClientSession::setDragSuppress(bool on)
	{
		if (m_dead || !m_channel)
		{
			return;
		}
		shell::ipc::v1::Envelope env;
		env.set_protocol(1);
		env.set_id(mps::ipc::newCorrelationId());
		env.set_dir(shell::ipc::v1::DIR_EVT);
		env.set_page_id(m_pageId);
		env.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
		env.mutable_set_drag_suppress()->set_suppress(on);
		m_channel->send(env);
	}

	void ClientSession::onEnvelope(shell::ipc::v1::Envelope env)
	{
		if (env.has_hello() && !m_helloSeen)
		{
			m_helloSeen = true;
			sendHelloAck();
			emit sessionHelloOk(this);
			return;
		}
		if (env.has_main_window_added())
		{
			m_mainWid = static_cast<quintptr>(env.main_window_added().wid());
			m_ready = true;
			emit sessionReady(this);
			return;
		}
		if (env.has_sub_window_added())
		{
			qint64 tabId = env.tab_id();
			if (tabId == 0 && !m_pendingTabs.isEmpty())
			{
				tabId = m_pendingTabs.takeFirst();
			}
			else if (!m_pendingTabs.isEmpty() && m_pendingTabs.front() == tabId)
			{
				m_pendingTabs.pop_front();
			}
			quintptr wid = static_cast<quintptr>(env.sub_window_added().wid());
			if (wid == 0)
			{
				wid = m_mainWid;
			}
			m_tabWids.insert(tabId, wid);
			const QString title = QString::fromStdString(env.sub_window_added().title());
			emit subWindowAdded(this, tabId, title, wid);
			return;
		}
		if (env.has_sub_window_removed())
		{
			const qint64 tabId = env.tab_id();
			m_tabWids.remove(tabId);
			emit subWindowRemoved(this, tabId);
			return;
		}
		if (env.has_query_close_sub_window_result())
		{
			// Accept → tear down Host tab immediately; SubWindowRemoved is idempotent backup.
			if (env.query_close_sub_window_result().accept())
			{
				const qint64 tabId = env.tab_id();
				m_tabWids.remove(tabId);
				emit subWindowRemoved(this, tabId);
			}
			return;
		}
		if (env.has_invoke())
		{
			// Client asks Host to create another window in this session.
			if (env.invoke().method() == "demo.request_new_window")
			{
				emit invokeNewWindow(this, env.tab_id());
				shell::ipc::v1::Envelope res;
				res.set_protocol(1);
				res.set_id(env.id());
				res.set_dir(shell::ipc::v1::DIR_RES);
				res.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
				res.mutable_invoke_result()->set_payload("ok");
				m_channel->send(res);
				return;
			}
			shell::ipc::v1::Envelope res;
			res.set_protocol(1);
			res.set_id(env.id());
			res.set_dir(shell::ipc::v1::DIR_RES);
			res.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
			auto* err = res.mutable_error();
			err->set_code(shell::ipc::v1::ERROR_UNIMPLEMENTED);
			err->set_message("Invoke not implemented in Demo Host");
			m_channel->send(res);
			return;
		}
		if (env.has_heartbeat())
		{
			return;
		}
	}
} // namespace mps::host
