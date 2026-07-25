#ifndef __MPS_TAB_STRIP_TAB_EMBED_MAP_H__
#define __MPS_TAB_STRIP_TAB_EMBED_MAP_H__

// Pure tabId → embed credential (wid) map. Host Tab model stays tabId-only;
// EmbedContainer is the only module that applies platform handles.

#include <cstdint>
#include <unordered_map>

namespace mps::tab_strip
{
	class TabEmbedMap
	{
	public:
		void bind(int64_t tabId, uint64_t wid)
		{
			if (tabId == 0 || wid == 0)
			{
				return;
			}
			m_byTab[tabId] = wid;
		}

		[[nodiscard]] bool has(int64_t tabId) const
		{
			return m_byTab.find(tabId) != m_byTab.end();
		}

		[[nodiscard]] uint64_t peek(int64_t tabId) const
		{
			const auto it = m_byTab.find(tabId);
			return it == m_byTab.end() ? 0 : it->second;
		}

		/// Remove and return wid; 0 if missing.
		uint64_t take(int64_t tabId)
		{
			const auto it = m_byTab.find(tabId);
			if (it == m_byTab.end())
			{
				return 0;
			}
			const uint64_t wid = it->second;
			m_byTab.erase(it);
			return wid;
		}

		void unbind(int64_t tabId)
		{
			m_byTab.erase(tabId);
		}

		void clear()
		{
			m_byTab.clear();
		}

		[[nodiscard]] std::size_t size() const
		{
			return m_byTab.size();
		}

	private:
		std::unordered_map<int64_t, uint64_t> m_byTab;
	};
} // namespace mps::tab_strip

#endif // __MPS_TAB_STRIP_TAB_EMBED_MAP_H__
