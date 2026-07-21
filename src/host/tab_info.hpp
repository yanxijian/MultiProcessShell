#ifndef __MPS_HOST_TAB_INFO_H__
#define __MPS_HOST_TAB_INFO_H__

#include <QString>

#include <cstdint>

namespace mps::host
{
	inline constexpr qint64 kHomeTabId = -1;

	/// Qt DnD mime for Host-only tab id (never put HWND in mime).
	inline constexpr char kTabMimeType[] = "application/x-mps-tab-id";

	struct TabInfo
	{
		qint64 pageId = 0;
		qint64 tabId = 0;
		int clientIndex = 0;
		int windowIndex = 0;
		QString title;
		quintptr wid = 0;
		class ClientSession* session = nullptr;
		bool isHome = false;

		static TabInfo makeHome()
		{
			TabInfo t;
			t.tabId = kHomeTabId;
			t.title = QStringLiteral("Home");
			t.isHome = true;
			return t;
		}
	};
} // namespace mps::host

#endif  // __MPS_HOST_TAB_INFO_H__
