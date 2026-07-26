#ifndef __MPS_IPC_HEARTBEAT_POLICY_H__
#define __MPS_IPC_HEARTBEAT_POLICY_H__

#include <cstdint>

namespace mps::ipc
{
	/// Spec §5.5 / Demo M6: Client→Host Heartbeat cadence and Host silence budget.
	inline constexpr std::int64_t kHeartbeatIntervalMs = 2000;
	inline constexpr std::int64_t kHeartbeatTimeoutMs = 6000;
	inline constexpr std::int64_t kHeartbeatWatchTickMs = 1000;

	/// @param lastBeatMs  last Heartbeat timestamp (same clock as @p nowMs)
	/// @param nowMs       current time
	/// @param timeoutMs   silence budget (default kHeartbeatTimeoutMs)
	[[nodiscard]] inline bool isHeartbeatTimedOut(std::int64_t lastBeatMs, std::int64_t nowMs, std::int64_t timeoutMs = kHeartbeatTimeoutMs)
	{
		if (lastBeatMs <= 0)
		{
			return false;
		}
		return (nowMs - lastBeatMs) >= timeoutMs;
	}
} // namespace mps::ipc

#endif // __MPS_IPC_HEARTBEAT_POLICY_H__
