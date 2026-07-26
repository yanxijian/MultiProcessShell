// demo_client sources (parent wires demos/CMakeLists.txt):
//   main.cpp, page_window.cpp, page_window.hpp, ribbon_page.cpp, ribbon_page.hpp
#include "client_app.hpp"
#include "qfluentribbon/ribbon_tokens.hpp"
#include "qfluentribbon/theme_bridge.hpp"
#include "qtheme/api.hpp"
#include "qtheme/engine.hpp"
#include "qtheme/store.hpp"
#include "qtheme/types.hpp"
#include "ribbon_page.hpp"

#include <QApplication>
#include <QColor>
#include <QCommandLineParser>
#include <QDir>

#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace
{
	void syncRibbonTokensFromEngine(qtheme::Engine* engine, qfluentribbon::ThemeBridge* bridge)
	{
		if (!engine || !bridge)
		{
			return;
		}
		qfluentribbon::tokens::setDpiScale(qtheme::api::dpiScale());

		auto pick = [engine](const QString& role, const QColor& fallback) -> QColor
		{
			if (qtheme::ThemeStore* store = engine->store())
			{
				const qtheme::ColorValue cv = store->color(QStringLiteral("palette"), role, fallback);
				return cv.ok ? cv.value : fallback;
			}
			return fallback;
		};

		bridge->ensureRibbonTokens(pick(QStringLiteral("window"), QColor(QStringLiteral("#F3F3F3"))),
								   pick(QStringLiteral("surface"), QColor(QStringLiteral("#FFFFFF"))),
								   pick(QStringLiteral("stroke"), QColor(QStringLiteral("#D1D1D1"))),
								   pick(QStringLiteral("text"), QColor(QStringLiteral("#1A1A1A"))),
								   pick(QStringLiteral("accent"), QColor(QStringLiteral("#0078D4"))),
								   pick(QStringLiteral("text.tertiary"), QColor(QStringLiteral("#8D8D8D"))),
								   pick(QStringLiteral("accent.text"), QColor(Qt::white)));
	}
} // namespace

int main(int argc, char* argv[])
{
#ifdef Q_OS_WIN
	// Align with Qt PMV2 before QApplication; keeps Host/Client embed DPI contexts matched.
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif
	QApplication app(argc, argv);
	QCommandLineParser parser;
	parser.addHelpOption();
	QCommandLineOption fromHost(QStringLiteral("from-host"));
	QCommandLineOption endpoint(QStringLiteral("endpoint"), QString(), QStringLiteral("name"));
	QCommandLineOption token(QStringLiteral("pipe-token"), QString(), QStringLiteral("token"));
	QCommandLineOption protocol(QStringLiteral("protocol"), QString(), QStringLiteral("n"), QStringLiteral("1"));
	QCommandLineOption noHeartbeat(QStringLiteral("no-heartbeat"), QStringLiteral("Disable Client Heartbeat (M6 repro)"));
	parser.addOption(fromHost);
	parser.addOption(endpoint);
	parser.addOption(token);
	parser.addOption(protocol);
	parser.addOption(noHeartbeat);
	parser.process(app);

	if (!parser.isSet(fromHost) || !parser.isSet(endpoint))
	{
		qWarning("demo_client must be started by the Host (--from-host --endpoint=...)");
		return 2;
	}

	qtheme::Engine engine;
	engine.apply(&app);
	qtheme::api::bind(&engine);
	qtheme::Engine::setDefault(&engine);
	qfluentribbon::ThemeBridge bridge;
	syncRibbonTokensFromEngine(&engine, &bridge);

	mps::client::PageFactory factory = [&engine, &bridge](qint64 tabId, const QString& title)
	{
		return std::make_unique<mps::demo::RibbonPage>(tabId, title, &engine, &bridge);
	};

	mps::client::ClientApp client(parser.value(endpoint), parser.value(token), std::move(factory), !parser.isSet(noHeartbeat));
	client.setAppearanceHandler(
		[&engine, &bridge](mps::theme::Scheme scheme)
		{
			const qtheme::ColorScheme cs = (scheme == mps::theme::Scheme::Dark) ? qtheme::ColorScheme::Dark : qtheme::ColorScheme::Light;
			(void)engine.setColorScheme(cs, /*force=*/true);
			syncRibbonTokensFromEngine(&engine, &bridge);
		});
	if (!client.connectToHost())
	{
		return 3;
	}
	return app.exec();
}
