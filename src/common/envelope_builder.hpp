#ifndef __MPS_IPC_ENVELOPE_BUILDER_H__
#define __MPS_IPC_ENVELOPE_BUILDER_H__

#include "shell/ipc/v1/ipc.pb.h"

#include <cstdint>
#include <string>
#include <utility>

namespace mps::ipc
{
	/// Fill Envelope header fields used by Demo Host / Client (body set by caller).
	[[nodiscard]] inline shell::ipc::v1::Envelope makeEnvelope(std::uint32_t protocol, std::string id, shell::ipc::v1::Dir dir,
															   std::int64_t tsMs, std::int64_t pageId = 0, std::int64_t tabId = 0)
	{
		shell::ipc::v1::Envelope env;
		env.set_protocol(protocol);
		env.set_id(std::move(id));
		env.set_dir(dir);
		env.set_ts_ms(tsMs);
		if (pageId != 0)
		{
			env.set_page_id(pageId);
		}
		if (tabId != 0)
		{
			env.set_tab_id(tabId);
		}
		return env;
	}

	/// RES that reuses the request correlation id (and optional tab).
	[[nodiscard]] inline shell::ipc::v1::Envelope makeResponse(std::uint32_t protocol, std::string requestId, std::int64_t tsMs,
															   std::int64_t tabId = 0)
	{
		return makeEnvelope(protocol, std::move(requestId), shell::ipc::v1::DIR_RES, tsMs, 0, tabId);
	}
} // namespace mps::ipc

#endif // __MPS_IPC_ENVELOPE_BUILDER_H__
