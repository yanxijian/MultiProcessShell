#ifndef __MPS_DEMOS_DEMO_SETTINGS_H__
#define __MPS_DEMOS_DEMO_SETTINGS_H__

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QString>

namespace mps::demos
{
	/// Demo prefs as INI under `<appDir>/config/<name>.ini`.
	[[nodiscard]] inline QSettings makeDemoSettings(const QString& fileBaseName)
	{
		const QString dir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config"));
		QDir().mkpath(dir);
		return QSettings(QDir(dir).filePath(fileBaseName + QStringLiteral(".ini")), QSettings::IniFormat);
	}
} // namespace mps::demos

#endif // __MPS_DEMOS_DEMO_SETTINGS_H__
