#ifndef __MPS_CLIENT_CLIENT_APP_H__
#define __MPS_CLIENT_CLIENT_APP_H__

#include "content_view.hpp"
#include "envelope_channel.hpp"
#include "theme_scheme.hpp"

#include <QHash>
#include <QLocalSocket>
#include <QObject>
#include <QTimer>

#include <functional>
#include <memory>
#include <mps/mps_client_export.hpp>

namespace mps::client
{
	class MPS_CLIENT_EXPORT ClientApp final : public QObject
	{
		Q_OBJECT
	public:
		using AppearanceHandler = std::function<void(mps::theme::Scheme)>;

		ClientApp(QString endpoint, QString token, ContentViewFactory factory, bool enableHeartbeat = true, QObject* parent = nullptr);
		~ClientApp() override;
		void connectToHost();
		/// Optional: apply process-wide appearance (e.g. QTE) before any view exists.
		void setAppearanceHandler(AppearanceHandler handler)
		{
			m_appearanceHandler = std::move(handler);
		}
		void setAppName(QString name)
		{
			m_appName = std::move(name);
		}
		void setRequestNewContentViewMethod(QString method)
		{
			m_requestNewContentViewMethod = std::move(method);
		}
		/// Optional: handle Host→Client Invoke methods other than theme.set.
		/// Return true if handled (fills payload or error). view may be null.
		using InvokeHandler =
			std::function<bool(ContentView* view, const QString& method, const QByteArray& params, QByteArray* payload, QString* error)>;
		void setInvokeHandler(InvokeHandler handler)
		{
			m_invokeHandler = std::move(handler);
		}

	signals:
		void connectionReady();
		void connectionFailed(QString reason);

	private:
		void applyThemeScheme(mps::theme::Scheme scheme);
		void requestThemeFromHost(mps::theme::Scheme scheme, qint64 tabId);
		void onEnvelope(mps::ipc::EnvelopePtr env);
		void sendHello();
		void sendHeartbeat();
		void startHeartbeatTimer();
		void stopHeartbeatTimer();
		void ensureMainWindowAddedSent();
		void createView(qint64 tabId, const QString& title);
		void closeView(qint64 tabId);
		void activateView(qint64 tabId);
		void failConnection(const QString& reason);

		QString m_endpoint;
		QString m_token;
		QString m_appName = QStringLiteral("client");
		QString m_requestNewContentViewMethod = QStringLiteral("demo.request_new_window");
		ContentViewFactory m_factory;
		AppearanceHandler m_appearanceHandler;
		InvokeHandler m_invokeHandler;
		bool m_enableHeartbeat = true;
		bool m_heartbeatArmed = false;
		bool m_connectionFailed = false;
		QLocalSocket* m_socket = nullptr;
		std::unique_ptr<mps::ipc::EnvelopeChannel> m_channel;
		QTimer* m_heartbeatTimer = nullptr;
		QTimer* m_handshakeTimer = nullptr;
		bool m_connectionReady = false;
		bool m_mainWindowAddedSent = false;
		QHash<qint64, ContentView*> m_views;
		ContentView* m_active = nullptr;
	};
} // namespace mps::client

#endif // __MPS_CLIENT_CLIENT_APP_H__
