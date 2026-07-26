#include "home_page.hpp"
#include "shell_app.hpp"
#include "theme_service.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char* argv[])
{
#ifdef Q_OS_WIN
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif
	QApplication app(argc, argv);
	QCoreApplication::setOrganizationName(QStringLiteral("yanxijian"));
	QCoreApplication::setApplicationName(QStringLiteral("mps_demo_host"));

	mps::demo_host::ThemeService theme;
	theme.start(&app);

	QString clientExe = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("mps_demo_client.exe"));
#ifndef Q_OS_WIN
	clientExe = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("mps_demo_client"));
#endif
	if (!QFileInfo::exists(clientExe))
	{
		qWarning("Client executable not found: %s", qPrintable(clientExe));
	}

	mps::host::ShellApp shellApp(clientExe, QStringLiteral("mps-demo"));
	shellApp.setShellWindowTitle(QStringLiteral("MultiProcessShell Demo"));
	shellApp.setRequestNewWindowMethod(QStringLiteral("demo.request_new_window"));
	shellApp.setHomeContentFactory(
		[&shellApp](mps::host::ShellWindow* shell) -> QWidget*
		{
			return new mps::demo_host::HomePage(&shellApp, shell);
		});
	QObject::connect(&shellApp, &mps::host::ShellApp::schemeChanged, &theme,
					 [&theme](mps::theme::Scheme scheme, mps::host::ThemeOrigin origin)
					 {
						 theme.applyScheme(scheme);
						 if (origin != mps::host::ThemeOrigin::Startup)
						 {
							 theme.persist(scheme);
						 }
					 });
	shellApp.setScheme(theme.scheme(), mps::host::ThemeOrigin::Startup);
	auto* shell = shellApp.createShell();
	(void)shell;
	return app.exec();
}
