#pragma once

#include <QString>
#include <cstdint>

namespace mps::host {

struct TabInfo {
  qint64 pageId = 0;
  qint64 tabId = 0;
  int clientIndex = 0;
  int windowIndex = 0;
  QString title;
  quintptr wid = 0;
  class ClientSession* session = nullptr;
};

}  // namespace mps::host
