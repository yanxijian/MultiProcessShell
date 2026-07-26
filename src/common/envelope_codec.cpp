#include "envelope_codec.hpp"

namespace mps::ipc
{
	void EnvelopeDeleter::operator()(shell::ipc::v1::Envelope* p) const noexcept
	{
		delete p;
	}

	EnvelopePtr createEnvelope()
	{
		return EnvelopePtr(new shell::ipc::v1::Envelope());
	}

	bool serializeEnvelope(const shell::ipc::v1::Envelope& env, std::string* out)
	{
		if (!out)
		{
			return false;
		}
		return env.SerializeToString(out);
	}

	EnvelopePtr parseEnvelope(std::string_view bytes)
	{
		auto env = createEnvelope();
		if (!env->ParseFromArray(bytes.data(), static_cast<int>(bytes.size())))
		{
			return {};
		}
		return env;
	}
} // namespace mps::ipc
