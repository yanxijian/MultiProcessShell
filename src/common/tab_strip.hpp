#pragma once

// Pure browser-style detachable tab strip / tear-out / shell rules (no Qt).
// Host widgets call these; unit tests lock the contracts against regressions.

#include <algorithm>
#include <cstdint>
#include <vector>

namespace mps::tab_strip {

namespace detail {
inline int min_i(int a, int b) { return a < b ? a : b; }
inline int max_i(int a, int b) { return a > b ? a : b; }
inline int clamp_i(int v, int lo, int hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}
}  // namespace detail

inline constexpr int64_t kHomeTabId = -1;

inline constexpr int kTabStripMargin = 8;
inline constexpr int kTabSpacing = 6;
inline constexpr int kDragInset = 16;

inline constexpr int kTearOutLeaveSlopV = 28;
inline constexpr int kTearOutLeaveSlopH = 10;
inline constexpr int kTearOutReturnSlopV = 10;
inline constexpr int kTearOutReturnSlopH = 4;

// --- Activation / MRU -------------------------------------------------------

inline void pushMru(std::vector<int64_t>& history, int64_t tabId) {
  history.erase(std::remove(history.begin(), history.end(), tabId), history.end());
  history.insert(history.begin(), tabId);
}

/// Next tab after closing `closingTabId`. Prefer MRU still present in `existingIds`,
/// else first non-Home client, else Home.
inline int64_t previousActivationTarget(const std::vector<int64_t>& history,
                                        const std::vector<int64_t>& existingIds,
                                        int64_t closingTabId) {
  const auto exists = [&](int64_t id) {
    return std::find(existingIds.begin(), existingIds.end(), id) != existingIds.end();
  };
  for (int64_t id : history) {
    if (id == closingTabId) {
      continue;
    }
    if (exists(id)) {
      return id;
    }
  }
  for (int64_t id : existingIds) {
    if (id != kHomeTabId && id != closingTabId) {
      return id;
    }
  }
  return kHomeTabId;
}

// --- Strip insert / Home pin ------------------------------------------------

/// Client tabs occupy [1, size]; Home stays at 0.
inline int clampClientInsertIndex(int insertIndex, int tabCount) {
  if (tabCount < 0) {
    tabCount = 0;
  }
  return detail::clamp_i(insertIndex, 1, tabCount);
}

inline bool isNoOpMove(int fromIndex, int insertIndex) {
  return insertIndex == fromIndex || insertIndex == fromIndex + 1;
}

/// After `takeAt(from)`, adjust the intended insert index.
inline int adjustInsertAfterTake(int fromIndex, int insertIndex) {
  if (insertIndex > fromIndex) {
    --insertIndex;
  }
  return insertIndex;
}

/// Midpoint insert from local X vs packed tab midpoints. Never returns 0
/// when there is at least a Home tab (caller passes widths including Home).
inline int midpointInsertIndex(int localX, const std::vector<int>& widths, int margin = kTabStripMargin,
                               int spacing = kTabSpacing) {
  if (widths.empty()) {
    return 1;
  }
  int insert = static_cast<int>(widths.size());
  int run = margin;
  for (int i = 0; i < static_cast<int>(widths.size()); ++i) {
    const int w = widths[static_cast<size_t>(i)];
    if (localX < run + w / 2) {
      insert = i;
      break;
    }
    run += w + spacing;
  }
  return detail::max_i(1, insert);
}

/// Ideal center X of `others[otherIndex]` when a gap of `dragW` is reserved at
/// `gapAmong` (layout origin = 0).
inline int idealOtherCenterX(const std::vector<int>& otherWidths, int dragW, int gapAmong,
                             int otherIndex, int spacing = kTabSpacing) {
  int x = 0;
  for (int i = 0; i < static_cast<int>(otherWidths.size()); ++i) {
    if (i == gapAmong) {
      x += dragW + spacing;
    }
    const int w = otherWidths[static_cast<size_t>(i)];
    if (i == otherIndex) {
      return x + w / 2;
    }
    x += w + spacing;
  }
  return x;
}

inline int dragInsetForWidth(int dragW, int maxInset = kDragInset) {
  return detail::min_i(maxInset, detail::max_i(1, dragW / 4));
}

/// Walk insert-among index until ghost edges (+/- inset) are stable vs neighbor centers.
inline int computeYieldInsertAmong(const std::vector<int>& otherWidths, int dragW, int ghostLeft,
                                   int ghostRight, int inset, int minAmong, int insertAmong,
                                   int spacing = kTabSpacing) {
  const int n = static_cast<int>(otherWidths.size());
  insertAmong = detail::clamp_i(insertAmong, minAmong, n);
  for (;;) {
    if (insertAmong > minAmong) {
      const int center =
          idealOtherCenterX(otherWidths, dragW, insertAmong, insertAmong - 1, spacing);
      if (ghostLeft + inset < center) {
        --insertAmong;
        continue;
      }
    }
    if (insertAmong < n) {
      const int center =
          idealOtherCenterX(otherWidths, dragW, insertAmong, insertAmong, spacing);
      if (ghostRight - inset > center) {
        ++insertAmong;
        continue;
      }
    }
    break;
  }
  return detail::clamp_i(insertAmong, minAmong, n);
}

inline std::vector<int64_t> buildYieldOrder(const std::vector<int64_t>& others, int insertAmong,
                                            int64_t dragTabId) {
  std::vector<int64_t> ids = others;
  insertAmong = detail::clamp_i(insertAmong, 0, static_cast<int>(ids.size()));
  ids.insert(ids.begin() + insertAmong, dragTabId);
  return ids;
}

/// Tear-out claim: siblings packed without the drag tab (no empty slot).
inline std::vector<int64_t> collapseSlotOrder(const std::vector<int64_t>& tabIds,
                                              int64_t dragTabId) {
  std::vector<int64_t> packed;
  packed.reserve(tabIds.size());
  for (int64_t id : tabIds) {
    if (id != dragTabId) {
      packed.push_back(id);
    }
  }
  return packed;
}

// --- Tear-out hysteresis / suppress -----------------------------------------

inline bool nextTearOutDetached(bool wasDetached, bool overStrip, bool nearLeave,
                                bool nearReturn) {
  if (wasDetached) {
    if (overStrip || nearReturn) {
      return false;
    }
    return true;
  }
  if (!overStrip && !nearLeave) {
    return true;
  }
  return false;
}

inline bool shouldSuppressTearOut(bool overAnyStrip, bool nearSourceLeaveSlop) {
  return overAnyStrip || nearSourceLeaveSlop;
}

// --- Merge / empty shell ----------------------------------------------------

inline bool canMergeTab(int64_t tabId, bool isHome, bool sourceExists, bool sameShell) {
  if (tabId == kHomeTabId || isHome) {
    return false;
  }
  if (!sourceExists || sameShell) {
    return false;
  }
  return true;
}

/// Destroy empty (Home-only) shell only when another top-level shell remains.
inline bool shouldDestroyEmptyShell(int clientTabCount, int shellCount) {
  return clientTabCount == 0 && shellCount > 1;
}

}  // namespace mps::tab_strip
