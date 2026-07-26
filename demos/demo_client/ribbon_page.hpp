#ifndef __MPS_DEMO_CLIENT_RIBBON_PAGE_H__
#define __MPS_DEMO_CLIENT_RIBBON_PAGE_H__

#include "client_page.hpp"
#include "page_window.hpp"

#include <memory>

namespace qfluentribbon
{
	class ThemeBridge;
}

namespace qtheme
{
	class Engine;
}

namespace mps::demo
{
	/// ClientPage adapter wrapping PageWindow (QFR Ribbon UI lives in the demo only).
	class RibbonPage final : public mps::client::ClientPage
	{
	public:
		RibbonPage(qint64 tabId, const QString& title, qtheme::Engine* engine, qfluentribbon::ThemeBridge* bridge);
		~RibbonPage() override;

		[[nodiscard]] QWidget* widget() override;
		[[nodiscard]] qint64 tabId() const override;
		void realizeChrome() override;
		void syncAfterEmbed() override;
		void applyTheme(mps::theme::Scheme scheme) override;

	private:
		void syncRibbonTokens();

		qtheme::Engine* m_engine = nullptr;
		qfluentribbon::ThemeBridge* m_bridge = nullptr;
		std::unique_ptr<PageWindow> m_window;
	};
} // namespace mps::demo

#endif // __MPS_DEMO_CLIENT_RIBBON_PAGE_H__
