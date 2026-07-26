#ifndef __MPS_IPC_ENVELOPE_BUILDER_H__
#define __MPS_IPC_ENVELOPE_BUILDER_H__

#include "envelope_codec.hpp"

#include <cstdint>
#include <string>

namespace mps::ipc
{
	/// Construct Envelope inside mps_ipc.dll (EnvelopePtr keeps destroy in-module).
	[[nodiscard]] MPS_IPC_EXPORT EnvelopePtr makeEnvelope(std::uint32_t protocol, std::string id, shell::ipc::v1::Dir dir,
														  std::int64_t tsMs, std::int64_t pageId = 0, std::int64_t tabId = 0);

	/// RES that reuses the request correlation id (and optional tab).
	[[nodiscard]] MPS_IPC_EXPORT EnvelopePtr makeResponse(std::uint32_t protocol, std::string requestId, std::int64_t tsMs,
														  std::int64_t tabId = 0);
} // namespace mps::ipc

#endif // __MPS_IPC_ENVELOPE_BUILDER_H__
