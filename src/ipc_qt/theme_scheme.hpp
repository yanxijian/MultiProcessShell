#ifndef __MPS_IPC_QT_THEME_SCHEME_H__
#define __MPS_IPC_QT_THEME_SCHEME_H__

#include <QByteArray>
#include <QMetaType>

namespace mps::theme
{
	/// Demo IPC ColorScheme wire values (`Invoke theme.set` params).
	enum class Scheme
	{
		Light,
		Dark,
	};

	[[nodiscard]] inline QByteArray toParams(Scheme scheme)
	{
		switch (scheme)
		{
		case Scheme::Dark:
			return QByteArrayLiteral("dark");
		case Scheme::Light:
		default:
			return QByteArrayLiteral("light");
		}
	}

	/// Accepts only trimmed `"light"` / `"dark"` (case-insensitive). Empty / other → false.
	[[nodiscard]] inline bool fromParams(const QByteArray& params, Scheme* out)
	{
		if (!out)
		{
			return false;
		}
		const QByteArray p = params.trimmed().toLower();
		if (p == QByteArrayLiteral("dark"))
		{
			*out = Scheme::Dark;
			return true;
		}
		if (p == QByteArrayLiteral("light"))
		{
			*out = Scheme::Light;
			return true;
		}
		return false;
	}
} // namespace mps::theme

Q_DECLARE_METATYPE(mps::theme::Scheme)

#endif // __MPS_IPC_QT_THEME_SCHEME_H__
