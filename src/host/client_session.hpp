#ifndef __MPS_HOST_CLIENT_SESSION_H__
#define __MPS_HOST_CLIENT_SESSION_H__

#include "envelope_channel.hpp"
#include "tab_info.hpp"
#include "theme_scheme.hpp"

#include <QByteArray>
#include <QHash>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>
#include <QTimer>

#include <functional>
#include <memory>
#include <mps/mps_host_export.hpp>

namespace mps::host
{
	class ShellWindow;

	class MPS_HOST_EXPORT ClientSession final : public QObject
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
		/// The optional callback fires exactly once on InvokeResult / RpcError /
		/// session death, so callers can roll back optimistic UI state.
		using InvokeCallback = std::function<void(bool ok, const QByteArray& payload, const QString& error)>;
		void sendInvoke(const QString& method, const QByteArray& params, qint64 tabId = 0, InvokeCallback callback = {});
		/// Deadline for an Invoke whose Client never answers (default 30 s).
		/// Mainly for tests / hosts that need faster failure feedback.
		void setInvokeTimeoutMs(int ms)
		{
			m_invokeTimeoutMs = ms > 0 ? ms : 1;
		}
		[[nodiscard]] bool isDead() const
		{
			return m_dead;
		}
		[[nodiscard]] bool helloSeen() const
		{
			return m_helloSeen;
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
		void onHandshakeTimeout();
		void failPendingInvokes(const QString& reason);
		void ensureInvokeWatch();
		void onInvokeWatchTick();

		struct PendingInvoke
		{
			InvokeCallback callback;
			qint64 deadlineMs = 0;
			QString method;
		};

		int m_instanceIndex = 0;
		qint64 m_sessionId = 0;
		QString m_endpoint;
		QString m_requestNewContentViewMethod;
		QString m_expectedAuthToken;
		bool m_ready = false;
		bool m_helloSeen = false;
		bool m_dead = false;
		bool m_unhealthy = false;
		bool m_heartbeatNegotiated = false;
		qint64 m_lastHeartbeatMs = 0;
		QTimer* m_heartbeatWatch = nullptr;
		QTimer* m_handshakeTimer = nullptr;
		QProcess* m_process = nullptr;
		QLocalSocket* m_socket = nullptr;
		std::unique_ptr<mps::ipc::EnvelopeChannel> m_channel;
		QHash<QString, PendingInvoke> m_invokeCallbacks;
		QTimer* m_invokeWatch = nullptr;
		int m_invokeTimeoutMs = 30000;
		// pending CreateSubWindow (proto) / ContentView tab ids awaiting SubWindowAdded (same order)
		QList<qint64> m_pendingTabs;
		quintptr m_embedRootWid = 0;
	};
} // namespace mps::host

#endif // __MPS_HOST_CLIENT_SESSION_H__
