#ifndef __MPS_HOST_TEAR_OUT_PREVIEW_H__
#define __MPS_HOST_TEAR_OUT_PREVIEW_H__

#include <QPixmap>
#include <QWidget>

namespace mps::host
{
	/// Small floating tab face that follows the cursor during same-shell reorder / merge.
	class TabDragGhost final : public QWidget
	{
		Q_OBJECT
	public:
		explicit TabDragGhost(QWidget* parent = nullptr);

		/// Logical content size of the tab face (shadow padding added internally).
		void setTabPixmap(const QPixmap& pm, QSize logicalContentSize);
		void setPixmap(const QPixmap& pm);
		/// Offset of content origin inside the widget (for hotspot adjustment).
		[[nodiscard]] QPoint contentOrigin() const
		{
			return m_contentOrigin;
		}
		[[nodiscard]] QSize contentSize() const
		{
			return m_contentSize;
		}

	protected:
		void paintEvent(QPaintEvent* event) override;

	private:
		QPixmap m_pm;
		QSize m_contentSize{120, 28};
		// Soft drop shadow below/right only (no top pad) so content top == widget top
		// and strip pinning does not look vertically biased.
		QPoint m_contentOrigin{6, 0};
	};

	/// Frameless translucent stand-in window that follows the cursor during tear-out
	/// (browser-style detachable-tab drag preview). Not a real shell — no embed.
	class TearOutPreview final : public QWidget
	{
		Q_OBJECT
	public:
		explicit TearOutPreview(QWidget* parent = nullptr);

		void setContentPixmap(const QPixmap& pm);
		/// Place this preview so its title/tab bar vertically centers (wraps) the
		/// floating tab content, and the tab sits at a Home+margin inset horizontally.
		void alignToTabContent(const QRect& globalTabContent);
		/// Same geometry math without moving (for drop → real shell).
		[[nodiscard]] static QRect geometryForTabContent(const QRect& globalTabContent, QSize previewSize);

		/// Must match paintEvent title bar (and ShellWindow title-bar height).
		static constexpr int kFramePad = 4;
		static constexpr int kTitleBarHeight = 40;
		/// Approx. left inset: title margin + Home tab + spacing.
		static constexpr int kTabLeftInset = 8 + 70 + 6;

	protected:
		void paintEvent(QPaintEvent* event) override;

	private:
		QPixmap m_content;
	};

	[[nodiscard]] QPixmap captureWindowPixmap(quintptr wid, QSize maxSize);
} // namespace mps::host

#endif  // __MPS_HOST_TEAR_OUT_PREVIEW_H__
