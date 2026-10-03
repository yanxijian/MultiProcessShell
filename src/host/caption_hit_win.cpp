#include "caption_hit_win.hpp"

#include "caption_hit.hpp"
#include "shell_window.hpp"

#include <QPushButton>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

namespace mps::host
{
	namespace
	{
		/// Process-wide NC hit pause flag; see CaptionHitPauseGuard.
		bool g_captionHitPaused = false;
	} // namespace

	CaptionHitPauseGuard::CaptionHitPauseGuard()
	{
		g_captionHitPaused = true;
	}

	CaptionHitPauseGuard::~CaptionHitPauseGuard()
	{
		g_captionHitPaused = false;
	}

	// ShellWindow member defined here on purpose: this file is the Win32 adapter
	// and keeps every windows.h detail (hit test, NC button visuals, actions)
	// out of shell_window.cpp. Non-Windows builds compile to a no-op entry.
	bool ShellWindow::nativeCaptionEvent(void* message, qintptr* result)
	{
#ifdef Q_OS_WIN
		if (g_captionHitPaused)
		{
			return false;
		}
		const auto* msg = static_cast<const MSG*>(message);
		if (!msg)
		{
			return false;
		}

		// Hit-test coordinates: WM_NCHITTEST asks about the lParam point, which
		// must stay authoritative (synthesized queries from automation/UIA can
		// carry any position, not just the cursor). Convert it through
		// ScreenToClient(), which operates in this process's DPI context
		// (virtualization-aware), then scale by the window's DPR to reach the
		// logical space of QWidget geometry. The previous approach (raw pixels
		// minus GetWindowRect ÷ DPR) broke under DPI virtualization (verified:
		// local points landed off by a full DPR factor after embedding a client
		// from a different DPI context). ScreenToClient on the top-level window
		// already subtracts the client origin, and this frameless window's
		// client origin equals its window origin (no non-client frame).
		const auto localPoint = [this, msg]() -> QPoint
		{
			const POINT pt = {GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
			POINT local = pt;
			ScreenToClient(msg->hwnd, &local);
			const qreal dpr = devicePixelRatioF() > 0.0 ? devicePixelRatioF() : 1.0;
			return QPoint(qRound(local.x / dpr), qRound(local.y / dpr));
		};

		const auto hitAt = [this, &localPoint]() -> CaptionHitPart
		{
			CaptionHitInput in;
			in.point = localPoint();
			in.windowSize = size();
			in.titleBarRect = m_hitTitleBarRect;
			in.minButtonRect = m_hitMinRect;
			in.maxButtonRect = m_hitMaxRect;
			in.closeButtonRect = m_hitCloseRect;
			in.interactiveRects = m_hitInteractiveRects;
			in.resizeBandThickness = m_hitBandThickness;
			in.maximized = isMaximized();
			in.fullScreen = isFullScreen();
			return captionHitTest(in);
		};

		const auto ncButton = [this](int idx) -> QPushButton*
		{
			switch (idx)
			{
			case 0:
				return m_minBtn;
			case 1:
				return m_maxBtn;
			case 2:
				return m_closeBtn;
			default:
				return nullptr;
			}
		};
		const auto buttonIndex = [](CaptionHitPart part) -> int
		{
			switch (part)
			{
			case CaptionHitPart::Minimize:
				return 0;
			case CaptionHitPart::Maximize:
				return 1;
			case CaptionHitPart::Close:
				return 2;
			default:
				return -1;
			}
		};
		// Idle / hover / pressed use standard QStyle states so any style (incl.
		// the theme engine) can distinguish them: WA_UnderMouse feeds
		// State_MouseOver and QAbstractButton::setDown feeds State_Sunken.
		// Client-side Enter/Leave never reach these buttons once they report
		// HT*BUTTON, hence this NC-driven state (plan risk R7).
		const auto applyVisual = [this, &ncButton](int idx)
		{
			if (QPushButton* b = ncButton(idx))
			{
				b->setAttribute(Qt::WA_UnderMouse, idx == m_ncHoverButton);
				b->setDown(idx == m_ncPressedButton);
				b->update();
			}
		};

		switch (msg->message)
		{
		case WM_NCHITTEST:
		{
			switch (hitAt())
			{
			case CaptionHitPart::Caption:
				*result = HTCAPTION;
				return true;
			case CaptionHitPart::Minimize:
				*result = HTMINBUTTON;
				return true;
			case CaptionHitPart::Maximize:
				*result = HTMAXBUTTON;
				return true;
			case CaptionHitPart::Close:
				*result = HTCLOSE;
				return true;
			case CaptionHitPart::EdgeLeft:
				*result = HTLEFT;
				return true;
			case CaptionHitPart::EdgeRight:
				*result = HTRIGHT;
				return true;
			case CaptionHitPart::EdgeTop:
				*result = HTTOP;
				return true;
			case CaptionHitPart::EdgeBottom:
				*result = HTBOTTOM;
				return true;
			case CaptionHitPart::CornerTopLeft:
				*result = HTTOPLEFT;
				return true;
			case CaptionHitPart::CornerTopRight:
				*result = HTTOPRIGHT;
				return true;
			case CaptionHitPart::CornerBottomLeft:
				*result = HTBOTTOMLEFT;
				return true;
			case CaptionHitPart::CornerBottomRight:
				*result = HTBOTTOMRIGHT;
				return true;
			case CaptionHitPart::None:
				break;
			}
			return false;
		}
		case WM_NCMOUSEMOVE:
		{
			const int idx = buttonIndex(hitAt());
			if (idx >= 0 && !m_ncTrackActive)
			{
				// WM_NCMOUSELEAVE only arrives after TrackMouseEvent; without it a
				// jump from a button straight into the client area leaks hover
				// (plan risk R8). Re-arms after every leave.
				TRACKMOUSEEVENT tme{};
				tme.cbSize = sizeof(tme);
				tme.dwFlags = TME_LEAVE | TME_NONCLIENT;
				tme.hwndTrack = msg->hwnd;
				TrackMouseEvent(&tme);
				m_ncTrackActive = true;
			}
			if (idx != m_ncHoverButton)
			{
				const int old = m_ncHoverButton;
				m_ncHoverButton = idx;
				if (old >= 0)
				{
					applyVisual(old);
				}
				if (idx >= 0)
				{
					applyVisual(idx);
				}
			}
			return false;
		}
		case WM_NCMOUSELEAVE:
		{
			m_ncTrackActive = false;
			if (m_ncHoverButton >= 0)
			{
				const int old = m_ncHoverButton;
				m_ncHoverButton = -1;
				applyVisual(old);
			}
			if (m_ncPressedButton >= 0)
			{
				const int old = m_ncPressedButton;
				m_ncPressedButton = -1;
				applyVisual(old);
			}
			return false;
		}
		case WM_NCLBUTTONDOWN:
		{
			// Consume button presses: DefWindowProc would enter its caption-button
			// modal loop and swallow the matching WM_NCLBUTTONUP — with no
			// WS_SYSMENU the system then performs nothing, leaving the click dead
			// (verified: a synthesized NC UP works, a physical click does not).
			// Consuming keeps the release on the normal hit-test path; releasing
			// off-button is handled by the UP case below (no action). The Win11
			// Snap flyout is hover-triggered and unaffected (R2 fallback).
			const CaptionHitPart part = hitAt();
			// Record the press part for the double-click handler: it must
			// classify by the press position (see WM_NCLBUTTONDBLCLK).
			m_ncPressPart = part;
			const int idx = buttonIndex(part);
			if (idx >= 0)
			{
				// Arm leave tracking here too: a press that jumps onto a button
				// without a preceding NC move (e.g. synthesized input) would
				// otherwise leak the pressed state if the pointer then leaves the
				// window entirely (no NCMOUSELEAVE without TrackMouseEvent).
				if (!m_ncTrackActive)
				{
					TRACKMOUSEEVENT tme{};
					tme.cbSize = sizeof(tme);
					tme.dwFlags = TME_LEAVE | TME_NONCLIENT;
					tme.hwndTrack = msg->hwnd;
					TrackMouseEvent(&tme);
					m_ncTrackActive = true;
				}
				if (idx != m_ncPressedButton)
				{
					const int old = m_ncPressedButton;
					m_ncPressedButton = idx;
					if (old >= 0)
					{
						applyVisual(old);
					}
				}
				applyVisual(idx);
				*result = 0;
				return true;
			}
			// Caption drag / edge resize still handled by the system.
			return false;
		}
		case WM_NCLBUTTONUP:
		{
			const CaptionHitPart part = hitAt();
			const int pressedIdx = m_ncPressedButton; // capture before clearing
			{
				const int oldHover = m_ncHoverButton;
				const int oldPressed = m_ncPressedButton;
				m_ncHoverButton = -1;
				m_ncPressedButton = -1;
				if (oldHover >= 0)
				{
					applyVisual(oldHover);
				}
				if (oldPressed >= 0 && oldPressed != oldHover)
				{
					applyVisual(oldPressed);
				}
			}
			// Gate the action on "released over the button that was pressed":
			// the second release of a double-click (WM_NCLBUTTONDBLCLK consumed
			// below, pressed not re-armed) and stray releases without a tracked
			// press must not toggle anything.
			const int releaseIdx = buttonIndex(part);
			if (releaseIdx < 0)
			{
				// Released off-button (e.g. inside the Win11 Snap flyout): do not
				// consume and do not toggle; the system finishes its part.
				return false;
			}
			if (pressedIdx != releaseIdx)
			{
				// Release without a matching press: consume to keep DefWindowProc
				// out of its button handling, but perform no action.
				*result = 0;
				return true;
			}
			switch (part)
			{
			case CaptionHitPart::Minimize:
				// Frameless windows carry no WS_SYSMENU / WS_MINIMIZEBOX: the system
				// does not perform the action, Host must (plan risk R1).
				showMinimized();
				*result = 0;
				return true;
			case CaptionHitPart::Close:
				close(); // closeEvent keeps routing through shellCloseRequested.
				*result = 0;
				return true;
			case CaptionHitPart::Maximize:
				isMaximized() ? showNormal() : showMaximized();
				*result = 0;
				return true;
			default:
				*result = 0;
				return true;
			}
		}
		case WM_NCLBUTTONDBLCLK:
		{
			// Classify by the PRESS part (m_ncPressPart), never by a fresh hit
			// test: the first release of the pair may already have toggled the
			// window state and moved the buttons away from under the cursor, so
			// re-testing would misread the old cursor position as caption blank.
			// Caption double-click toggles maximize. A double-click on a system
			// button is consumed with no action: the first click already acted on
			// WM_NCLBUTTONUP, and the second release is gated there by the
			// missing press state — net effect is exactly one toggle.
			const CaptionHitPart pressPart = m_ncPressPart;
			m_ncPressPart = CaptionHitPart::None;
			if (pressPart == CaptionHitPart::Caption)
			{
				isMaximized() ? showNormal() : showMaximized();
				*result = 0;
				return true;
			}
			if (buttonIndex(pressPart) >= 0)
			{
				*result = 0;
				return true;
			}
			return false;
		}
		default:
			return false;
		}
#else
		Q_UNUSED(message);
		Q_UNUSED(result);
		return false;
#endif
	}
} // namespace mps::host
