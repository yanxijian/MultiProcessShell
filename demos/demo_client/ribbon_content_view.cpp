#include "ribbon_content_view.hpp"

#include "qfluentribbon/ribbon_bar.hpp"
#include "qfluentribbon/ribbon_tokens.hpp"
#include "qfluentribbon/theme_bridge.hpp"
#include "qtheme/api.hpp"
#include "qtheme/engine.hpp"
#include "qtheme/store.hpp"
#include "qtheme/types.hpp"

#include <QColor>

namespace mps::demo
{
	RibbonContentView::RibbonContentView(qint64 tabId, const QString& title, qtheme::Engine* engine, qfluentribbon::ThemeBridge* bridge)
		: m_engine(engine)
		, m_bridge(bridge)
		, m_window(std::make_unique<RibbonContentWindow>(tabId, title, bridge))
	{
		QObject::connect(m_window.get(), &RibbonContentWindow::requestNewWindow, m_window.get(),
						 [this]()
						 {
							 if (onRequestNewWindow)
							 {
								 onRequestNewWindow();
							 }
						 });
		QObject::connect(m_window.get(), &RibbonContentWindow::requestThemeScheme, m_window.get(),
						 [this](mps::theme::Scheme scheme)
						 {
							 if (onRequestTheme)
							 {
								 onRequestTheme(scheme);
							 }
						 });
	}

	RibbonContentView::~RibbonContentView() = default;

	QWidget* RibbonContentView::widget()
	{
		return m_window.get();
	}

	qint64 RibbonContentView::tabId() const
	{
		return m_window ? m_window->tabId() : 0;
	}

	void RibbonContentView::realizeChrome()
	{
		if (m_window)
		{
			m_window->realizeChrome();
			if (m_window->ribbonBar())
			{
				m_window->ribbonBar()->polishFromStore();
			}
		}
	}

	void RibbonContentView::syncAfterEmbed()
	{
		if (m_window)
		{
			m_window->syncAfterEmbed();
		}
	}

	void RibbonContentView::syncRibbonTokens()
	{
		if (!m_engine || !m_bridge)
		{
			return;
		}
		qfluentribbon::tokens::setDpiScale(qtheme::api::dpiScale());

		auto pick = [this](const QString& role, const QColor& fallback) -> QColor
		{
			if (qtheme::ThemeStore* store = m_engine->store())
			{
				const qtheme::ColorValue cv = store->color(QStringLiteral("palette"), role, fallback);
				return cv.ok ? cv.value : fallback;
			}
			return fallback;
		};

		m_bridge->ensureRibbonTokens(pick(QStringLiteral("window"), QColor(QStringLiteral("#F3F3F3"))),
									 pick(QStringLiteral("surface"), QColor(QStringLiteral("#FFFFFF"))),
									 pick(QStringLiteral("stroke"), QColor(QStringLiteral("#D1D1D1"))),
									 pick(QStringLiteral("text"), QColor(QStringLiteral("#1A1A1A"))),
									 pick(QStringLiteral("accent"), QColor(QStringLiteral("#0078D4"))),
									 pick(QStringLiteral("text.tertiary"), QColor(QStringLiteral("#8D8D8D"))),
									 pick(QStringLiteral("accent.text"), QColor(Qt::white)));
	}

	void RibbonContentView::applyTheme(mps::theme::Scheme scheme)
	{
		if (!m_engine)
		{
			return;
		}
		qtheme::ColorScheme cs = qtheme::ColorScheme::Light;
		if (scheme == mps::theme::Scheme::Dark)
		{
			cs = qtheme::ColorScheme::Dark;
		}
		(void)m_engine->setColorScheme(cs, /*force=*/true);
		syncRibbonTokens();
		if (m_window && m_window->ribbonBar())
		{
			m_window->ribbonBar()->polishFromStore();
		}
	}
} // namespace mps::demo
