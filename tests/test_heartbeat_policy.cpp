#include "heartbeat_policy.hpp"

#include <gtest/gtest.h>

using mps::ipc::isHeartbeatTimedOut;
using mps::ipc::kHeartbeatIntervalMs;
using mps::ipc::kHeartbeatTimeoutMs;

TEST(HeartbeatPolicy, ConstantsMatchSpec)
{
	EXPECT_EQ(kHeartbeatIntervalMs, 2000);
	EXPECT_EQ(kHeartbeatTimeoutMs, 6000);
	EXPECT_GE(kHeartbeatTimeoutMs, 3 * kHeartbeatIntervalMs);
}

TEST(HeartbeatPolicy, TimedOutAfterBudget)
{
	EXPECT_FALSE(isHeartbeatTimedOut(1000, 1000 + 5999));
	EXPECT_TRUE(isHeartbeatTimedOut(1000, 1000 + 6000));
	EXPECT_TRUE(isHeartbeatTimedOut(1000, 1000 + 9000));
}

TEST(HeartbeatPolicy, NoBeatYetIsNotTimedOut)
{
	EXPECT_FALSE(isHeartbeatTimedOut(0, 100000));
	EXPECT_FALSE(isHeartbeatTimedOut(-1, 100000));
}
