#include "tab_strip.hpp"

#include <gtest/gtest.h>
#include <vector>

using mps::tab_strip::kHomeTabId;
using mps::tab_strip::previousActivationTarget;
using mps::tab_strip::pushMru;

TEST(TabStripMru, PushMovesToFrontAndDedupes)
{
	std::vector<int64_t> hist{1, 2, 3};
	pushMru(hist, 2);
	EXPECT_EQ(hist, (std::vector<int64_t>{2, 1, 3}));
	pushMru(hist, 9);
	EXPECT_EQ(hist, (std::vector<int64_t>{9, 2, 1, 3}));
}

TEST(TabStripMru, CloseActiveRestoresPrevious)
{
	// Activate Home → A → B → C; close C → B.
	std::vector<int64_t> hist;
	pushMru(hist, kHomeTabId);
	pushMru(hist, 10);
	pushMru(hist, 20);
	pushMru(hist, 30);
	const std::vector<int64_t> existing{kHomeTabId, 10, 20}; // 30 already removed
	EXPECT_EQ(previousActivationTarget(hist, existing, 30), 20);
}

TEST(TabStripMru, CloseFallsBackToHomeWhenOnlyHomeLeft)
{
	std::vector<int64_t> hist{20, 10, kHomeTabId};
	const std::vector<int64_t> existing{kHomeTabId};
	EXPECT_EQ(previousActivationTarget(hist, existing, 20), kHomeTabId);
}

TEST(TabStripMru, CloseInactiveDoesNotNeedHistoryHit)
{
	// When history entries are stale, fall back to first remaining client.
	std::vector<int64_t> hist{99, 88}; // neither still present except we close 20
	const std::vector<int64_t> existing{kHomeTabId, 10};
	EXPECT_EQ(previousActivationTarget(hist, existing, 20), 10);
}
