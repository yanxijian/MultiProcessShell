#ifndef __MPS_DEMO_CLIENT_RIBBON_CONTENT_VIEW_H__
#define __MPS_DEMO_CLIENT_RIBBON_CONTENT_VIEW_H__

#include "content_view.hpp"
#include "ribbon_content_window.hpp"

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
	/// ContentView adapter wrapping RibbonContentWindow (QFR Ribbon UI lives in the demo only).
	class RibbonContentView final : public mps::client::ContentView
	{
	public:
		RibbonContentView(qint64 tabId, const QString& title, qtheme::Engine* engine, qfluentribbon::ThemeBridge* bridge);
		~RibbonContentView() override;

		[[nodiscard]] QWidget* widget() override;
		[[nodiscard]] qint64 tabId() const override;
		void realizeChrome() override;
		void syncAfterEmbed() override;
		void applyTheme(mps::theme::Scheme scheme) override;

	private:
		void syncRibbonTokens();

		qtheme::Engine* m_engine = nullptr;
		qfluentribbon::ThemeBridge* m_bridge = nullptr;
		std::unique_ptr<RibbonContentWindow> m_window;
	};
} // namespace mps::demo

#endif // __MPS_DEMO_CLIENT_RIBBON_CONTENT_VIEW_H__
