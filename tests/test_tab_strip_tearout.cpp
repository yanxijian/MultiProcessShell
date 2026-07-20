#include "tab_strip.hpp"

#include <gtest/gtest.h>

using namespace mps::tab_strip;

TEST(TabStripTearOut, LeaveRequiresExitBeyondLeaveSlop)
{
	// Attached: over strip → stay attached.
	EXPECT_FALSE(nextTearOutDetached(/*was=*/false,
									 /*over=*/true,
									 /*nearLeave=*/true,
									 /*nearReturn=*/false));
	// Attached: outside strip but still in leave slop → stay attached.
	EXPECT_FALSE(nextTearOutDetached(false, false, /*nearLeave=*/true, false));
	// Attached: outside strip and leave slop → detach.
	EXPECT_TRUE(nextTearOutDetached(false, false, /*nearLeave=*/false, false));
}

TEST(TabStripTearOut, ReturnUsesTighterSlop)
{
	// Detached: near return slop → re-attach.
	EXPECT_FALSE(nextTearOutDetached(/*was=*/true,
									 /*over=*/false,
									 /*nearLeave=*/false,
									 /*nearReturn=*/true));
	// Detached: over strip → re-attach.
	EXPECT_FALSE(nextTearOutDetached(true, /*over=*/true, false, false));
	// Detached: far away (not over, not near return) → stay detached.
	EXPECT_TRUE(nextTearOutDetached(true, false, /*nearLeave=*/true, /*nearReturn=*/false));
}

TEST(TabStripTearOut, SuppressWhenOverStripOrNearSource)
{
	EXPECT_TRUE(shouldSuppressTearOut(true, false));
	EXPECT_TRUE(shouldSuppressTearOut(false, true));
	EXPECT_FALSE(shouldSuppressTearOut(false, false));
}

TEST(TabStripTearOut, LeaveSlopWiderThanReturn)
{
	EXPECT_GT(kTearOutLeaveSlopV, kTearOutReturnSlopV);
	EXPECT_GT(kTearOutLeaveSlopH, kTearOutReturnSlopH);
}
