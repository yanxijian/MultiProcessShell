#pragma once

#include <QString>
#include <cstdint>

namespace mps::host {

inline constexpr qint64 kHomeTabId = -1;

struct TabInfo {
  qint64 pageId = 0;
  qint64 tabId = 0;
  int clientIndex = 0;
  int windowIndex = 0;
  QString title;
  quintptr wid = 0;
  class ClientSession* session = nullptr;
  bool isHome = false;

  static TabInfo makeHome() {
    TabInfo t;
    t.tabId = kHomeTabId;
    t.title = QStringLiteral("Home");
    t.isHome = true;
    return t;
  }
};

}  // namespace mps::host
