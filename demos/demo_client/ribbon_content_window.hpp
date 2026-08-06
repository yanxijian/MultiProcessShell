#ifndef __MPS_DEMO_CLIENT_RIBBON_CONTENT_WINDOW_H__
#define __MPS_DEMO_CLIENT_RIBBON_CONTENT_WINDOW_H__

#include "qfluentribbon/ribbon_window.hpp"
#include "theme_scheme.hpp"

#include <QString>

namespace qfluentribbon
{
	class ThemeBridge;
}

namespace mps::demo
{
	/// Frameless Ribbon page reported to the Host via SubWindowAdded (HWND embed).
	class RibbonContentWindow final : public qfluentribbon::RibbonWindow
	{
		Q_OBJECT
	public:
		RibbonContentWindow(qint64 tabId, QString title, qfluentribbon::ThemeBridge* bridge, QWidget* parent = nullptr);
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
		void requestThemeScheme(mps::theme::Scheme scheme);

	protected:
		bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

	private:
		void buildRibbon(qfluentribbon::ThemeBridge* bridge);

		qint64 m_tabId = 0;
		bool m_embedSyncPending = false;
		bool m_embedSynced = false;
		bool m_chromeReady = false;
		qfluentribbon::ThemeBridge* m_pendingBridge = nullptr;
	};
} // namespace mps::demo

#endif // __MPS_DEMO_CLIENT_RIBBON_CONTENT_WINDOW_H__
