#ifndef __MPS_HOST_CAPTION_HIT_H__
#define __MPS_HOST_CAPTION_HIT_H__

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QVector>

namespace mps::host
{
	/// Which part of a frameless shell window a point lands on.
	/// Pure geometry rule: QtCore only, no windows.h, no widget access —
	/// unit-testable on every platform (see tests/test_caption_hit.cpp).
	enum class CaptionHitPart
	{
		None, ///< Do not intercept; let the message reach Qt / DefWindowProc.
		Caption,
		Minimize,
		Maximize,
		Close,
		EdgeLeft,
		EdgeRight,
		EdgeTop,
		EdgeBottom,
		CornerTopLeft,
		CornerTopRight,
		CornerBottomLeft,
		CornerBottomRight,
	};

	struct CaptionHitInput
	{
		/// Point in window-local logical coordinates (same space as QWidget geometry).
		QPoint point;
		/// Window size in logical coordinates (drives the resize band).
		QSize windowSize;
		/// Title bar rect, window-local logical coordinates.
		QRect titleBarRect;
		/// System caption button rects, window-local logical coordinates.
		QRect minButtonRect;
		QRect maxButtonRect;
		QRect closeButtonRect;
		/// Rects that must keep receiving client-side mouse events (tab buttons;
		/// a tab's rect covers its close button). Lower priority than system buttons.
		QVector<QRect> interactiveRects;
		/// Resize band thickness around the window edges; 0 disables resizing.
		int resizeBandThickness = 0;
		bool maximized = false;
		bool fullScreen = false;
		/// While a QDrag::exec loop runs, everything reports None (client area).
		bool paused = false;
	};

	/// Judgment order (design doc "命中规则"):
	/// 1. paused → None; maximized/fullscreen force the band thickness to 0.
	/// 2. Resize band (corners beat edges) beats caption and buttons.
	/// 3. Outside the title bar → None (content area / embedded child HWNDs stay client).
	/// 4. System buttons (buttons beat interactive rects).
	/// 5. Interactive rects → None.
	/// 6. Remaining title bar area → Caption, except fullscreen → None.
	[[nodiscard]] inline CaptionHitPart captionHitTest(const CaptionHitInput& input)
	{
		if (input.paused)
		{
			return CaptionHitPart::None;
		}

		// Points outside the window never hit anything (WM_NCHITTEST only asks
		// about points inside the window; guards the band logic below).
		if (input.windowSize.width() <= 0 || input.windowSize.height() <= 0 || input.point.x() < 0 || input.point.y() < 0
			|| input.point.x() >= input.windowSize.width() || input.point.y() >= input.windowSize.height())
		{
			return CaptionHitPart::None;
		}

		const int thickness = (input.maximized || input.fullScreen) ? 0 : input.resizeBandThickness;
		if (thickness > 0)
		{
			const bool left = input.point.x() < thickness;
			const bool right = input.point.x() >= input.windowSize.width() - thickness;
			const bool top = input.point.y() < thickness;
			const bool bottom = input.point.y() >= input.windowSize.height() - thickness;
			if (top && left)
			{
				return CaptionHitPart::CornerTopLeft;
			}
			if (top && right)
			{
				return CaptionHitPart::CornerTopRight;
			}
			if (bottom && left)
			{
				return CaptionHitPart::CornerBottomLeft;
			}
			if (bottom && right)
			{
				return CaptionHitPart::CornerBottomRight;
			}
			if (left)
			{
				return CaptionHitPart::EdgeLeft;
			}
			if (right)
			{
				return CaptionHitPart::EdgeRight;
			}
			if (top)
			{
				return CaptionHitPart::EdgeTop;
			}
			if (bottom)
			{
				return CaptionHitPart::EdgeBottom;
			}
		}

		if (!input.titleBarRect.contains(input.point))
		{
			return CaptionHitPart::None;
		}
		if (input.closeButtonRect.contains(input.point))
		{
			return CaptionHitPart::Close;
		}
		if (input.maxButtonRect.contains(input.point))
		{
			return CaptionHitPart::Maximize;
		}
		if (input.minButtonRect.contains(input.point))
		{
			return CaptionHitPart::Minimize;
		}
		for (const QRect& rect : input.interactiveRects)
		{
			if (rect.contains(input.point))
			{
				return CaptionHitPart::None;
			}
		}
		return input.fullScreen ? CaptionHitPart::None : CaptionHitPart::Caption;
	}
} // namespace mps::host

#endif // __MPS_HOST_CAPTION_HIT_H__
