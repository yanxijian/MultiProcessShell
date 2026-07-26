#ifndef __MPS_IPC_ENVELOPE_CODEC_H__
#define __MPS_IPC_ENVELOPE_CODEC_H__

#include "shell/ipc/v1/ipc.pb.h"

#include <memory>
#include <mps/mps_ipc_export.hpp>
#include <string>
#include <string_view>

namespace mps::ipc
{
	/// Destroy Envelope in the same module that allocated it (mps_ipc.dll).
	/// Generated Message types live in mps_ipc; even with shared libprotobuf.dll,
	/// constructing/destroying them in another DLL is unsafe on MSVC.
	struct MPS_IPC_EXPORT EnvelopeDeleter
	{
		void operator()(shell::ipc::v1::Envelope* p) const noexcept;
	};

	using EnvelopePtr = std::unique_ptr<shell::ipc::v1::Envelope, EnvelopeDeleter>;

	[[nodiscard]] MPS_IPC_EXPORT bool serializeEnvelope(const shell::ipc::v1::Envelope& env, std::string* out);
	[[nodiscard]] MPS_IPC_EXPORT EnvelopePtr parseEnvelope(std::string_view bytes);
	[[nodiscard]] MPS_IPC_EXPORT EnvelopePtr createEnvelope();
} // namespace mps::ipc

#endif // __MPS_IPC_ENVELOPE_CODEC_H__
