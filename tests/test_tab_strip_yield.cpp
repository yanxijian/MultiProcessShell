#include "tab_strip.hpp"

#include <gtest/gtest.h>

#include <vector>

using namespace mps::tab_strip;

TEST(TabStripYield, ClampNeverBeforeHome) {
  EXPECT_EQ(clampClientInsertIndex(0, 3), 1);
  EXPECT_EQ(clampClientInsertIndex(-5, 3), 1);
  EXPECT_EQ(clampClientInsertIndex(2, 3), 2);
  EXPECT_EQ(clampClientInsertIndex(99, 3), 3);
  EXPECT_EQ(clampClientInsertIndex(1, 1), 1);  // Home-only shell
}

TEST(TabStripYield, MidpointNeverInsertsBeforeHome) {
  // [Home=80][A=100][B=100], margin 8, spacing 6
  const std::vector<int> widths{80, 100, 100};
  // Cursor over Home midpoint → would be 0, clamped to 1.
  EXPECT_EQ(midpointInsertIndex(8 + 40, widths), 1);
  // Past Home mid into A → still 1 (before A).
  EXPECT_EQ(midpointInsertIndex(8 + 80 + 6 + 10, widths), 1);
  // Past A mid → 2.
  EXPECT_EQ(midpointInsertIndex(8 + 80 + 6 + 50, widths), 2);
  // Past B mid → 3 (append).
  EXPECT_EQ(midpointInsertIndex(8 + 80 + 6 + 100 + 6 + 80, widths), 3);
}

TEST(TabStripYield, NoOpMoveSameSlot) {
  EXPECT_TRUE(isNoOpMove(2, 2));
  EXPECT_TRUE(isNoOpMove(2, 3));
  EXPECT_FALSE(isNoOpMove(2, 1));
  EXPECT_FALSE(isNoOpMove(2, 4));
}

TEST(TabStripYield, AdjustInsertAfterTake) {
  EXPECT_EQ(adjustInsertAfterTake(1, 3), 2);
  EXPECT_EQ(adjustInsertAfterTake(2, 1), 1);
}

TEST(TabStripYield, YieldInsertRespectsHomeFloorAndInset) {
  // others = Home, A, B — widths 80,100,100; drag W=100; minAmong=1
  const std::vector<int> others{80, 100, 100};
  const int dragW = 100;
  const int inset = dragInsetForWidth(dragW);
  EXPECT_EQ(inset, 16);

  // Ghost far left still cannot go before Home (minAmong=1).
  int among = computeYieldInsertAmong(others, dragW, /*ghostLeft=*/-1000,
                                      /*ghostRight=*/-900, inset, /*minAmong=*/1,
                                      /*insertAmong=*/2);
  EXPECT_EQ(among, 1);

  // Ghost far right → append after B.
  among = computeYieldInsertAmong(others, dragW, 2000, 2100, inset, 1, 1);
  EXPECT_EQ(among, 3);
}

TEST(TabStripYield, BuildYieldOrderInsertsDrag) {
  const std::vector<int64_t> others{kHomeTabId, 10, 20};
  const auto order = buildYieldOrder(others, 2, 99);
  EXPECT_EQ(order, (std::vector<int64_t>{kHomeTabId, 10, 99, 20}));
}

TEST(TabStripYield, CollapseSlotRemovesDragGap) {
  const std::vector<int64_t> ids{kHomeTabId, 10, 20, 30};
  EXPECT_EQ(collapseSlotOrder(ids, 20), (std::vector<int64_t>{kHomeTabId, 10, 30}));
}
