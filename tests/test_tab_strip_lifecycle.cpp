#include "tab_strip.hpp"

#include <gtest/gtest.h>

using namespace mps::tab_strip;

TEST(TabStripLifecycle, MergeRejectsHomeSameShellMissing)
{
	EXPECT_FALSE(canMergeTab(kHomeTabId, true, true, false));
	EXPECT_FALSE(canMergeTab(10, false, false, false)); // no source
	EXPECT_FALSE(canMergeTab(10, false, true, true));	// same shell
	EXPECT_TRUE(canMergeTab(10, false, true, false));
}

TEST(TabStripLifecycle, EmptyShellDestroyOnlyWhenNotLast)
{
	EXPECT_FALSE(shouldDestroyEmptyShell(/*clientTabs=*/1, /*shells=*/2));
	EXPECT_FALSE(shouldDestroyEmptyShell(0, 1)); // keep last Home-only shell
	EXPECT_TRUE(shouldDestroyEmptyShell(0, 2));
	EXPECT_TRUE(shouldDestroyEmptyShell(0, 5));
}
