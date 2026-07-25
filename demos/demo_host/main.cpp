#include "shell_app.hpp"

#include <QApplication>
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
	QString clientExe = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("mps_demo_client.exe"));
#ifndef Q_OS_WIN
	clientExe = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("mps_demo_client"));
#endif
	if (!QFileInfo::exists(clientExe))
	{
		qWarning("Client executable not found: %s", qPrintable(clientExe));
	}
	mps::host::ShellApp shellApp(clientExe);
	shellApp.createShell();
	return app.exec();
}
