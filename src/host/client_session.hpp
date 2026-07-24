#ifndef __MPS_HOST_CLIENT_SESSION_H__
#define __MPS_HOST_CLIENT_SESSION_H__

#include "envelope_channel.hpp"
#include "tab_info.hpp"

#include <QHash>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>

#include <memory>

namespace mps::host
{
	class ShellWindow;

	class ClientSession final : public QObject
	{
		Q_OBJECT
	public:
		ClientSession(int clientIndex, QString endpoint, QObject* parent = nullptr);
		~ClientSession() override;

		[[nodiscard]] int clientIndex() const
		{
			return m_clientIndex;
		}
		[[nodiscard]] qint64 pageId() const
		{
			return m_pageId;
		}
		[[nodiscard]] bool ready() const
		{
			return m_ready;
		}
		[[nodiscard]] mps::ipc::EnvelopeChannel* channel()
		{
			return m_channel.get();
		}

		void startClientProcess(const QString& clientExe, const QString& token);
		void attachSocket(QLocalSocket* socket);
		void requestCreateSubWindow(qint64 tabId, const QString& title);
		void requestActivate(qint64 tabId);
		void requestClose(qint64 tabId);
		void notifyReattachment(qint64 shellId);
		void setDragSuppress(bool on);
		[[nodiscard]] bool isDead() const
		{
			return m_dead;
		}

	signals:
		void sessionHelloOk(ClientSession* self);
		void sessionReady(ClientSession* self);
		void subWindowAdded(ClientSession* self, qint64 tabId, QString title, quintptr wid);
		void subWindowRemoved(ClientSession* self, qint64 tabId);
		void sessionDead(ClientSession* self);
		void invokeNewWindow(ClientSession* self, qint64 sourceTabId);

	private:
		void onEnvelope(shell::ipc::v1::Envelope env);
		void sendHelloAck();
		void markDead();

		int m_clientIndex = 0;
		qint64 m_pageId = 0;
		QString m_endpoint;
		bool m_ready = false;
		bool m_helloSeen = false;
		bool m_dead = false;
		QProcess* m_process = nullptr;
		QLocalSocket* m_socket = nullptr;
		std::unique_ptr<mps::ipc::EnvelopeChannel> m_channel;
		// pending CreateSubWindow tab ids awaiting SubWindowAdded (same order)
		QList<qint64> m_pendingTabs;
		QHash<qint64, quintptr> m_tabWids;
		quintptr m_mainWid = 0;
	};
} // namespace mps::host

#endif  // __MPS_HOST_CLIENT_SESSION_H__
