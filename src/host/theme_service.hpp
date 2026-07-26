#ifndef __MPS_HOST_THEME_SERVICE_H__
#define __MPS_HOST_THEME_SERVICE_H__

#include "qtheme/engine.hpp"
#include "qtheme/types.hpp"
#include "theme_scheme.hpp"

#include <QObject>

#include <memory>

class QApplication;

namespace mps::host
{
	enum class ThemeOrigin
	{
		Startup,
		HostUi,
		ClientRequest,
	};

	[[nodiscard]] inline qtheme::ColorScheme toColorScheme(mps::theme::Scheme scheme)
	{
		return scheme == mps::theme::Scheme::Dark ? qtheme::ColorScheme::Dark : qtheme::ColorScheme::Light;
	}

	[[nodiscard]] inline mps::theme::Scheme toThemeScheme(qtheme::ColorScheme scheme)
	{
		return scheme == qtheme::ColorScheme::Dark ? mps::theme::Scheme::Dark : mps::theme::Scheme::Light;
	}

	/// Process-wide appearance SSOT for the Demo Host (Light/Dark).
	class ThemeService final : public QObject
	{
		Q_OBJECT
	public:
		explicit ThemeService(QObject* parent = nullptr);

		void start(QApplication* app);
		[[nodiscard]] qtheme::Engine* engine() const
		{
			return m_engine.get();
		}
		[[nodiscard]] qtheme::ColorScheme scheme() const;
		/// Apply locally, persist, and emit schemeChanged when the scheme actually changes.
		void setScheme(qtheme::ColorScheme scheme, ThemeOrigin origin);

	signals:
		void schemeChanged(qtheme::ColorScheme scheme, ThemeOrigin origin);

	private:
		void persist() const;
		void loadOrDefault();

		std::unique_ptr<qtheme::Engine> m_engine;
	};
} // namespace mps::host

#endif // __MPS_HOST_THEME_SERVICE_H__
