#include "theme_service.hpp"

#include "demo_settings.hpp"
#include "qtheme/api.hpp"

#include <QApplication>

namespace mps::demo_host
{
	namespace
	{
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
		applyScheme(loadPersistedOrDefault());
	}

	mps::theme::Scheme ThemeService::scheme() const
	{
		return m_engine ? toThemeScheme(m_engine->colorScheme()) : mps::theme::Scheme::Light;
	}

	void ThemeService::applyScheme(mps::theme::Scheme scheme)
	{
		if (!m_engine)
		{
			return;
		}
		(void)m_engine->setColorScheme(toColorScheme(scheme), /*force=*/true);
	}

	mps::theme::Scheme ThemeService::loadPersistedOrDefault() const
	{
		QSettings settings = mps::demos::makeDemoSettings(QStringLiteral("mps_demo_host"));
		const QByteArray raw = settings.value(QString::fromUtf8(kSchemeKey), QStringLiteral("light")).toString().toUtf8();
		mps::theme::Scheme wire = mps::theme::Scheme::Light;
		if (!mps::theme::fromParams(raw, &wire))
		{
			wire = mps::theme::Scheme::Light;
		}
		return wire;
	}

	void ThemeService::persist(mps::theme::Scheme scheme) const
	{
		QSettings settings = mps::demos::makeDemoSettings(QStringLiteral("mps_demo_host"));
		settings.setValue(QString::fromUtf8(kSchemeKey), QString::fromUtf8(mps::theme::toParams(scheme)));
	}
} // namespace mps::demo_host
