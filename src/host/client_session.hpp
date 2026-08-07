#ifndef __MPS_HOST_CLIENT_SESSION_H__
#define __MPS_HOST_CLIENT_SESSION_H__

#include "envelope_channel.hpp"
#include "tab_info.hpp"
#include "theme_scheme.hpp"

#include <QByteArray>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>
#include <QTimer>

#include <memory>

namespace mps::host
{
	class ShellWindow;

	class ClientSession final : public QObject
	{
		Q_OBJECT
	public:
		ClientSession(int instanceIndex, QString endpoint, QString requestNewContentViewMethod = QStringLiteral("demo.request_new_window"),
					  QObject* parent = nullptr);
		~ClientSession() override;

		[[nodiscard]] int instanceIndex() const
		{
			return m_instanceIndex;
		}
		[[nodiscard]] qint64 sessionId() const
		{
			return m_sessionId;
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
		void requestCreateContentView(qint64 tabId, const QString& title);
		void requestActivate(qint64 tabId);
		void requestClose(qint64 tabId);
		void notifyReattachment(qint64 shellId);
		void setDragSuppress(bool on);
		/// Host → Client: push global ColorScheme ("light" / "dark").
		void pushThemeScheme(const QByteArray& params);
		/// Host → Client: opaque Invoke (e.g. Volition open_document).
		void sendInvoke(const QString& method, const QByteArray& params, qint64 tabId = 0);
		[[nodiscard]] bool isDead() const
		{
			return m_dead;
		}
		[[nodiscard]] bool isUnhealthy() const
		{
			return m_unhealthy;
		}
		/// User-confirmed kill of the Client process (M6). Existing sessionDead path cleans tabs.
		void terminateProcess();

	signals:
		void sessionHelloOk(ClientSession* self);
		void sessionReady(ClientSession* self);
		void contentViewReady(ClientSession* self, qint64 tabId, QString title, quintptr wid);
		void contentViewClosed(ClientSession* self, qint64 tabId);
		void sessionDead(ClientSession* self);
		void sessionUnhealthy(ClientSession* self);
		void sessionHealthy(ClientSession* self);
		void createContentViewRequested(ClientSession* self, qint64 sourceTabId);
		/// Validated C→H theme.set (wire scheme already parsed).
		void themeSetRequested(ClientSession* self, mps::theme::Scheme scheme);
		/// Client → Host: update Host tab label for tabId.
		void tabTitleChanged(ClientSession* self, qint64 tabId, QString title);

	private:
		void onEnvelope(mps::ipc::EnvelopePtr env);
		void sendHelloAck();
		void markDead();
		void noteHeartbeat();
		void startHeartbeatWatch();
		void stopHeartbeatWatch();
		void onHeartbeatWatchTick();

		int m_instanceIndex = 0;
		qint64 m_sessionId = 0;
		QString m_endpoint;
		QString m_requestNewContentViewMethod;
		bool m_ready = false;
		bool m_helloSeen = false;
		bool m_dead = false;
		bool m_unhealthy = false;
		bool m_heartbeatNegotiated = false;
		qint64 m_lastHeartbeatMs = 0;
		QTimer* m_heartbeatWatch = nullptr;
		QProcess* m_process = nullptr;
		QLocalSocket* m_socket = nullptr;
		std::unique_ptr<mps::ipc::EnvelopeChannel> m_channel;
		// pending CreateSubWindow (proto) / ContentView tab ids awaiting SubWindowAdded (same order)
		QList<qint64> m_pendingTabs;
		quintptr m_embedRootWid = 0;
	};
} // namespace mps::host

#endif // __MPS_HOST_CLIENT_SESSION_H__
