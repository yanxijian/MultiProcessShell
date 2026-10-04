#ifndef __MPS_HOST_TAB_INFO_H__
#define __MPS_HOST_TAB_INFO_H__

#include <QCoreApplication>
#include <QString>

#include <cstdint>

namespace mps::host
{
	inline constexpr qint64 kHomeTabId = -1;

	/// Qt DnD mime for Host-only tab id (never put HWND in mime).
	inline constexpr char kTabMimeType[] = "application/x-mps-tab-id";

	struct TabInfo
	{
		qint64 sessionId = 0;
		qint64 tabId = 0;
		int instanceIndex = 0;
		int contentIndex = 0;
		QString title;
		class ClientSession* session = nullptr;
		bool isHome = false;
		bool unhealthy = false;
		/// Client process exited; tab kept with a Host-side fault page (Chrome-style).
		bool crashed = false;

		static TabInfo makeHome()
		{
			TabInfo t;
			t.tabId = kHomeTabId;
			t.title = QStringLiteral("Home");
			t.isHome = true;
			return t;
		}

		[[nodiscard]] QString displayTitle() const
		{
			if (isHome)
			{
				return title;
			}
			if (crashed)
			{
				return title + QCoreApplication::translate("mps::host::TabInfo", " (Crashed)");
			}
			if (unhealthy)
			{
				return title + QCoreApplication::translate("mps::host::TabInfo", " (Not responding)");
			}
			return title;
		}

		/// Full-page Host placeholder is for dead Clients only.
		/// Unhealthy (heartbeat timeout) keeps the embed visible — Spec M6 is
		/// tab suffix + terminate, not an interstitial that hides the HWND.
		[[nodiscard]] bool needsFaultPage() const
		{
			return !isHome && crashed;
		}
	};
} // namespace mps::host

#endif // __MPS_HOST_TAB_INFO_H__
