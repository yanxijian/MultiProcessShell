#include "envelope_builder.hpp"

namespace mps::ipc
{
	EnvelopePtr makeEnvelope(std::uint32_t protocol, std::string id, shell::ipc::v1::Dir dir, std::int64_t tsMs, std::int64_t pageId,
							 std::int64_t tabId)
	{
		auto env = createEnvelope();
		env->set_protocol(protocol);
		env->set_id(std::move(id));
		env->set_dir(dir);
		env->set_ts_ms(tsMs);
		if (pageId != 0)
		{
			env->set_page_id(pageId);
		}
		if (tabId != 0)
		{
			env->set_tab_id(tabId);
		}
		return env;
	}

	EnvelopePtr makeResponse(std::uint32_t protocol, std::string requestId, std::int64_t tsMs, std::int64_t tabId)
	{
		return makeEnvelope(protocol, std::move(requestId), shell::ipc::v1::DIR_RES, tsMs, 0, tabId);
	}
} // namespace mps::ipc
