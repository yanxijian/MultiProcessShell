#ifndef __MPS_CLIENT_CLIENT_APP_H__
#define __MPS_CLIENT_CLIENT_APP_H__

#include "envelope_channel.hpp"
#include "qfluentribbon/ribbon_window.hpp"
#include "qfluentribbon/theme_bridge.hpp"
#include "qtheme/engine.hpp"

#include <QHash>
#include <QLocalSocket>
#include <QObject>
#include <QTimer>

#include <memory>

namespace mps::client
{
	/// Frameless Ribbon page reported to the Host via SubWindowAdded (HWND embed).
	class PageWindow final : public qfluentribbon::RibbonWindow
	{
		Q_OBJECT
	public:
		PageWindow(qint64 tabId, QString title, qfluentribbon::ThemeBridge* bridge, qtheme::Engine* engine, QWidget* parent = nullptr);
		[[nodiscard]] qint64 tabId() const
		{
			return m_tabId;
		}
		/// Bind HWND to the primary screen, then build ribbon/icons.
		void realizeChrome();
		/// Match QWidget size to the embedded HWND and rebuild the backing store.
		void syncAfterEmbed();

	signals:
		void requestNewWindow();

	protected:
		bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

	private:
		void buildRibbon(qfluentribbon::ThemeBridge* bridge, qtheme::Engine* engine);

		qint64 m_tabId = 0;
		bool m_embedSyncPending = false;
		bool m_embedSynced = false;
		bool m_chromeReady = false;
		qfluentribbon::ThemeBridge* m_pendingBridge = nullptr;
		qtheme::Engine* m_pendingEngine = nullptr;
	};

	class ClientApp final : public QObject
	{
		Q_OBJECT
	public:
		ClientApp(QString endpoint, QString token, bool enableHeartbeat = true, QObject* parent = nullptr);
		~ClientApp() override;
		[[nodiscard]] bool connectToHost();

	private:
		void ensureTheme();
		void onEnvelope(shell::ipc::v1::Envelope env);
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
		bool m_enableHeartbeat = true;
		bool m_heartbeatArmed = false;
		QLocalSocket* m_socket = nullptr;
		std::unique_ptr<mps::ipc::EnvelopeChannel> m_channel;
		QTimer* m_heartbeatTimer = nullptr;
		bool m_mainReported = false;
		std::unique_ptr<qtheme::Engine> m_engine;
		std::unique_ptr<qfluentribbon::ThemeBridge> m_bridge;
		QHash<qint64, PageWindow*> m_pages;
		PageWindow* m_active = nullptr;
	};
} // namespace mps::client

#endif // __MPS_CLIENT_CLIENT_APP_H__
