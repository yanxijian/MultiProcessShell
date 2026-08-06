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

namespace mps::client
{
	class ClientApp final : public QObject
	{
		Q_OBJECT
	public:
		using AppearanceHandler = std::function<void(mps::theme::Scheme)>;

		ClientApp(QString endpoint, QString token, ContentViewFactory factory, bool enableHeartbeat = true, QObject* parent = nullptr);
		~ClientApp() override;
		[[nodiscard]] bool connectToHost();
		/// Optional: apply process-wide appearance (e.g. QTE) before any view exists.
		void setAppearanceHandler(AppearanceHandler handler)
		{
			m_appearanceHandler = std::move(handler);
		}
		void setAppName(QString name)
		{
			m_appName = std::move(name);
		}
		void setRequestNewWindowMethod(QString method)
		{
			m_requestNewWindowMethod = std::move(method);
		}

	private:
		void applyThemeScheme(mps::theme::Scheme scheme);
		void requestThemeFromHost(mps::theme::Scheme scheme, qint64 tabId);
		void onEnvelope(mps::ipc::EnvelopePtr env);
		void sendHello();
		void sendHeartbeat();
		void startHeartbeatTimer();
		void stopHeartbeatTimer();
		void ensureMainReported();
		void createView(qint64 tabId, const QString& title);
		void closeView(qint64 tabId);
		void activateView(qint64 tabId);

		QString m_endpoint;
		QString m_token;
		QString m_appName = QStringLiteral("client");
		QString m_requestNewWindowMethod = QStringLiteral("shell.request_new_window");
		ContentViewFactory m_factory;
		AppearanceHandler m_appearanceHandler;
		bool m_enableHeartbeat = true;
		bool m_heartbeatArmed = false;
		QLocalSocket* m_socket = nullptr;
		std::unique_ptr<mps::ipc::EnvelopeChannel> m_channel;
		QTimer* m_heartbeatTimer = nullptr;
		bool m_mainReported = false;
		QHash<qint64, ContentView*> m_views;
		ContentView* m_active = nullptr;
	};
} // namespace mps::client

#endif // __MPS_CLIENT_CLIENT_APP_H__
