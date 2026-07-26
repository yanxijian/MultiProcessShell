#include "theme_service.hpp"

#include "qtheme/api.hpp"

#include <QApplication>
#include <QSettings>

namespace mps::host
{
	namespace
	{
		constexpr auto kOrg = "yanxijian";
		constexpr auto kApp = "mps_demo_host";
		constexpr auto kSchemeKey = "appearance/colorScheme";
	} // namespace

	ThemeService::ThemeService(QObject* parent)
		: QObject(parent)
	{
	}

	void ThemeService::start(QApplication* app)
	{
		if (!app || m_engine)
		{
			return;
		}
		m_engine = std::make_unique<qtheme::Engine>();
		m_engine->apply(app);
		qtheme::api::bind(m_engine.get());
		qtheme::Engine::setDefault(m_engine.get());
		loadOrDefault();
	}

	qtheme::ColorScheme ThemeService::scheme() const
	{
		return m_engine ? m_engine->colorScheme() : qtheme::ColorScheme::Light;
	}

	void ThemeService::setScheme(qtheme::ColorScheme scheme, ThemeOrigin origin)
	{
		if (!m_engine)
		{
			return;
		}
		if (scheme != qtheme::ColorScheme::Light && scheme != qtheme::ColorScheme::Dark)
		{
			scheme = qtheme::ColorScheme::Light;
		}
		const qtheme::ColorScheme previous = m_engine->colorScheme();
		if (previous == scheme)
		{
			return;
		}
		(void)m_engine->setColorScheme(scheme, /*force=*/true);
		persist();
		if (origin != ThemeOrigin::Startup)
		{
			emit schemeChanged(scheme, origin);
		}
	}

	void ThemeService::persist() const
	{
		if (!m_engine)
		{
			return;
		}
		QSettings settings(QString::fromUtf8(kOrg), QString::fromUtf8(kApp));
		settings.setValue(QString::fromUtf8(kSchemeKey), QString::fromUtf8(mps::theme::toParams(toThemeScheme(m_engine->colorScheme()))));
	}

	void ThemeService::loadOrDefault()
	{
		QSettings settings(QString::fromUtf8(kOrg), QString::fromUtf8(kApp));
		const QByteArray raw = settings.value(QString::fromUtf8(kSchemeKey), QStringLiteral("light")).toString().toUtf8();
		mps::theme::Scheme wire = mps::theme::Scheme::Light;
		if (!mps::theme::fromParams(raw, &wire))
		{
			wire = mps::theme::Scheme::Light;
		}
		(void)m_engine->setColorScheme(toColorScheme(wire), /*force=*/true);
	}
} // namespace mps::host
