#ifndef __MPS_HOST_EMBED_CONTAINER_H__
#define __MPS_HOST_EMBED_CONTAINER_H__

#include "tab_embed_map.hpp"

#include <QPixmap>
#include <QWidget>

#include <cstdint>

namespace mps::host
{
	/// Native container that hosts a Client HWND via SetParent (Windows).
	/// Tab model talks only in tabId; wid stays inside this embed seam.
	class EmbedContainer final : public QWidget
	{
		Q_OBJECT
	public:
		explicit EmbedContainer(QWidget* parent = nullptr);

		void bind(qint64 tabId, quintptr wid);
		void unbind(qint64 tabId);
		[[nodiscard]] bool has(qint64 tabId) const;
		/// Remove binding and stop tracking if it is the active Client window (no Hide).
		[[nodiscard]] quintptr takeBinding(qint64 tabId);
		/// Move binding from `from` to `to` without Hide (tear-out / merge handoff).
		static quintptr transferBinding(EmbedContainer* from, EmbedContainer* to, qint64 tabId);

		void activate(qint64 tabId);
		void clearActive(bool hide = true);
		/// Stop tracking active HWND without Hide (tear-out/merge handoff).
		void releaseActive();
		void releaseActiveIfTab(qint64 tabId);
		/// Drop all bindings and release the active Client window (shell teardown).
		void reset();

		void resyncActive();
		[[nodiscard]] QPixmap grabContent(qint64 tabId, QSize maxSize);
		[[nodiscard]] quintptr activeClientWindow() const
		{
			return m_clientWid;
		}

	protected:
		void resizeEvent(QResizeEvent* event) override;
		void showEvent(QShowEvent* event) override;

	private:
		void clearClientWindow(bool hide);
		void releaseClientWindow();
		void setClientWindow(quintptr wid);
		void syncClientGeometry();
		void applyEmbed();
		[[nodiscard]] bool clientWindowAlive() const;

		mps::tab_strip::TabEmbedMap m_bindings;
		quintptr m_clientWid = 0;
		qint64 m_activeTabId = 0;
#ifdef Q_OS_WIN
		unsigned long m_attachedClientThreadId = 0;
#endif
	};
} // namespace mps::host

#endif // __MPS_HOST_EMBED_CONTAINER_H__
