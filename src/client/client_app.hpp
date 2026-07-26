#ifndef __MPS_CLIENT_CLIENT_APP_H__
#define __MPS_CLIENT_CLIENT_APP_H__

#include "client_page.hpp"
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

		ClientApp(QString endpoint, QString token, PageFactory factory, bool enableHeartbeat = true, QObject* parent = nullptr);
		~ClientApp() override;
		[[nodiscard]] bool connectToHost();
		/// Optional: apply process-wide appearance (e.g. QTE) before any page exists.
		void setAppearanceHandler(AppearanceHandler handler)
		{
			m_appearanceHandler = std::move(handler);
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
		void createPage(qint64 tabId, const QString& title);
		void closePage(qint64 tabId);
		void activatePage(qint64 tabId);

		QString m_endpoint;
		QString m_token;
		PageFactory m_factory;
		AppearanceHandler m_appearanceHandler;
		bool m_enableHeartbeat = true;
		bool m_heartbeatArmed = false;
		QLocalSocket* m_socket = nullptr;
		std::unique_ptr<mps::ipc::EnvelopeChannel> m_channel;
		QTimer* m_heartbeatTimer = nullptr;
		bool m_mainReported = false;
		QHash<qint64, ClientPage*> m_pages;
		ClientPage* m_active = nullptr;
	};
} // namespace mps::client

#endif // __MPS_CLIENT_CLIENT_APP_H__
