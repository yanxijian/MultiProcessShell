#ifndef __MPS_HOST_SHELL_WINDOW_H__
#define __MPS_HOST_SHELL_WINDOW_H__

#include "caption_hit.hpp"
#include "embed_container.hpp"
#include "tab_info.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QMainWindow>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QVector>

#include <memory>

namespace mps::host
{
	class ClientSession;
	class ShellApp;

	class TabButton final : public QFrame
	{
		Q_OBJECT
	public:
		TabButton(const TabInfo& info, QWidget* parent = nullptr);
		[[nodiscard]] const TabInfo& info() const
		{
			return m_info;
		}
		void setInfo(const TabInfo& info);
		void setActive(bool on);
		/// Re-read app palette (QTE-driven in demos) into owner-drawn tab chrome.
		void refreshChrome();

	signals:
		void closeRequested(qint64 tabId);
		void activated(qint64 tabId);
		void terminateSessionRequested(ClientSession* session);
		/// localHotSpot: press position inside the tab (for grab offset).
		void dragStarted(qint64 tabId, QPoint localHotSpot);

	protected:
		void paintEvent(QPaintEvent* event) override;
		void resizeEvent(QResizeEvent* event) override;
		void mousePressEvent(QMouseEvent* event) override;
		void mouseMoveEvent(QMouseEvent* event) override;
		void mouseReleaseEvent(QMouseEvent* event) override;
		bool eventFilter(QObject* watched, QEvent* event) override;
		[[nodiscard]] QSize sizeHint() const override;

	private:
		void updateTitlePalette();
		void updateElidedTitle();

		TabInfo m_info;
		QLabel* m_title = nullptr;
		QPushButton* m_closeBtn = nullptr;
		QPoint m_dragStart;
		bool m_active = false;
		bool m_pressActive = false;
		bool m_dragging = false;
	};

	class ShellWindow final : public QMainWindow
	{
		Q_OBJECT
	public:
		explicit ShellWindow(ShellApp* app, QWidget* parent = nullptr);

		[[nodiscard]] qint64 shellId() const
		{
			return m_shellId;
		}
		void addTab(const TabInfo& info);
		void insertTab(const TabInfo& info, int insertIndex);
		void moveTab(qint64 tabId, int insertIndex);
		void removeTab(qint64 tabId);
		void setActiveTab(qint64 tabId);
		/// Update the Host tab label for `tabId`.
		void setTabTitle(qint64 tabId, const QString& title);
		[[nodiscard]] QVector<TabInfo> tabs() const
		{
			return m_tabs;
		}
		[[nodiscard]] qint64 activeTabId() const
		{
			return m_activeTabId;
		}
		[[nodiscard]] int clientTabCount() const;
		[[nodiscard]] EmbedContainer* embedContainer()
		{
			return m_embed;
		}
		[[nodiscard]] QWidget* titleBarWidget() const
		{
			return m_titleBar;
		}
		/// Tab buttons + trailing strip (merge/reorder hot zone); excludes window buttons.
		[[nodiscard]] bool isOverTabDropZone(QPoint globalPos) const;
		[[nodiscard]] bool isNearTabDropZone(QPoint globalPos, int verticalSlop, int horizontalSlop) const;
		/// Min/max/close — not a valid drop target during tab drag.
		[[nodiscard]] bool isOverWindowButtons(QPoint globalPos) const;
		[[nodiscard]] QRect tabStripGlobalRect() const;
		/// Local Y of tab buttons inside the title bar (centered, not hard-coded top).
		[[nodiscard]] int tabStripContentY() const;
		/// Top Y of the tab row in global coords (for locking reorder ghost vertically).
		[[nodiscard]] int tabRowTopGlobal() const;
		[[nodiscard]] bool isStripDropTarget(const QObject* watched) const;
		[[nodiscard]] int tabInsertIndexAt(QPoint globalPos) const;
		void updateDropInsertIndicator(int insertIndex);
		void clearDropInsertIndicator();
		/// Live reorder/merge yield from cursor (model unchanged until drop).
		/// guestWidth > 0: drag tab is not in this shell (merge into target).
		/// hotSpotX: ghost grab offset; <0 means center.
		void previewTabYieldAtCursor(qint64 dragTabId, QPoint globalPos, int guestWidth = 0, int hotSpotX = -1);
		/// Apply yieldOrder_ to the tab model (same-shell drop). Returns true if applied.
		bool commitTabYieldPreview();
		/// Stop the yield preview. keepDragTabHidden: leave the dragged tab's
		/// button invisible (tear-out path — the tab is about to be removed;
		/// unhiding it first makes it flash in the strip for one repaint).
		void clearTabYieldPreview(bool keepDragTabHidden = false);
		/// Tear-out: siblings immediately claim the vacated strip slot (no gap).
		void collapseTornOutTabSlot(qint64 dragTabId);
		[[nodiscard]] bool hasTabYieldPreview() const
		{
			return m_yieldDragTabId != 0;
		}
		/// Insert index of the dragged tab in the live yield order (-1 if none).
		[[nodiscard]] int yieldInsertIndex() const;
		/// Global rect of the drag slot (for cancel snap-back).
		[[nodiscard]] QRect tabDragSlotGlobalRect(qint64 tabId) const;
		/// Stop tracking active HWND without Hide (tear-out/merge handoff).
		void releaseEmbedTrackingForTab(qint64 tabId);
		/// Keep layout slot but make the dragged tab invisible.
		void setTabDragHidden(qint64 tabId, bool hidden);
		[[nodiscard]] qint64 previousActivationTarget(qint64 closingTabId) const;
		[[nodiscard]] QPixmap grabTabButton(qint64 tabId) const;
		/// Logical size of a tab button (for drag ghost hotspot on high-DPI).
		[[nodiscard]] QSize tabButtonSize(qint64 tabId) const;
		void installStripDropFilter(QObject* filter);
		/// Replace Home client-area content (framework owns only the empty slot by default).
		void setHomeContent(QWidget* content);
		void takeTabsFrom(ShellWindow* other, const QList<qint64>& tabIds);
		/// Close without emitting shellCloseRequested (app-driven teardown).
		void forceClose();
		/// Refresh tab labels / styles for a session's health (M6).
		void setSessionUnhealthy(ClientSession* session, bool unhealthy);
		/// Re-apply title bar / Home slot / tab chrome from the app palette (QTE in demos).
		void applyThemeChrome();
		/// Force DWM to rebuild this window's frame. Needed after Qt removes
		/// WS_EX_LAYERED (setWindowOpacity back to 1.0 after a drag): the layered
		/// toggle on a THICKFRAME window with a custom region can leave DWM
		/// composing a stale frame (verified: dark band around the window with an
		/// intact client render). Idempotent, cheap (no move/size/activate).
		void refreshNativeFrame();
		/// Full render-surface self-heal for transient native-state damage.
		/// After DPI / geometry transients (embed SetParent storms, modal
		/// activation changes, layered toggles, WM_DPICHANGED churn) the pad
		/// band inside the border ring can hold stale DWM-surface pixels that
		/// Qt's damage tracking considers clean, so ordinary repaints never
		/// overwrite them (observed: a constant ~8px foreign-color band just
		/// inside the border ring, its color drifting with window history).
		/// Re-applies the rounded region (re-masks the Win10 THICKFRAME DWM
		/// border), forces a synchronous full repaint + flush of the root (a
		/// plain update() would be dropped by the stale damage region), then
		/// rebuilds the DWM frame. Idempotent; call after any operation that
		/// toggles window opacity, geometry, or embedding.
		void healRenderSurface();

	signals:
		void tabCloseRequested(qint64 tabId);
		void tabActivated(qint64 tabId);
		void tabTearOutRequested(qint64 tabId, QRect suggestedGeometry);
		void tabMergeRequested(qint64 tabId, ShellWindow* target, int insertIndex);
		void terminateSessionRequested(ClientSession* session);
		void shellCloseRequested(ShellWindow* self);
		void dropIndicatorsClearRequested();

	protected:
		void showEvent(QShowEvent* event) override;
		void changeEvent(QEvent* event) override;
		void resizeEvent(QResizeEvent* event) override;
		void closeEvent(QCloseEvent* event) override;
		bool event(QEvent* event) override;
		bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
		bool eventFilter(QObject* watched, QEvent* event) override;

	private:
		void rebuildTabs();
		void syncEmbedToActive();
		void syncWorkspace();
		void pushActivationHistory(qint64 tabId);
		void reinstallStripDropTargets();
		void scheduleEmbedResync();
		/// Coalesced healRenderSurface() on the next event-loop tick (used from
		/// event handlers where an immediate synchronous repaint is unsafe).
		void scheduleRenderHeal();
		void ensureStripDragLayout(qint64 hideTabId, int guestWidth = 0);
		void animateTabGeometry(TabButton* btn, const QRect& target);
		void stopTabSlideAnimations();
		void updateFrameChrome();
		/// NC hit-test rect cache (consumed by caption_hit_win.cpp). Coalesced to
		/// the next event-loop tick so child geometry is final when read.
		void scheduleCaptionHitCacheRefresh();
		void refreshCaptionHitCache();
		/// Win32 non-client hit-test / caption-button adapter, defined in
		/// caption_hit_win.cpp; no-op returning false on non-Windows builds.
		bool nativeCaptionEvent(void* message, qintptr* result);
		/// Rounded window clip at native pixel resolution.
		/// Windows: SetWindowRgn. macOS: CALayer cornerRadius. Other: QPA mask
		/// in native pixels (not QWidget::setMask, which HiDPI-quantizes per rect).
		/// Called from updateFrameChrome.
		void applyNativeWindowRegion();
		[[nodiscard]] int frameRadius() const;
		[[nodiscard]] int frameBorderWidth() const;
		[[nodiscard]] QColor frameBorderColor() const;
		TabInfo* findTab(qint64 tabId);
		[[nodiscard]] const TabInfo* findTab(qint64 tabId) const;

		ShellApp* m_app = nullptr;
		QObject* m_stripDropFilter = nullptr;
		qint64 m_shellId = 0;
		QWidget* m_root = nullptr;
		QVBoxLayout* m_rootLay = nullptr;
		QWidget* m_titleBar = nullptr;
		QFrame* m_titleBarSep = nullptr;
		QWidget* m_tabDropTrail = nullptr; // trailing strip: drop-to-append + caption drag
		QWidget* m_dropIndicator = nullptr;
		QPushButton* m_minBtn = nullptr;
		QPushButton* m_maxBtn = nullptr;
		QPushButton* m_closeBtn = nullptr;
		QHBoxLayout* m_tabRow = nullptr;
		QStackedWidget* m_stack = nullptr;
		QWidget* m_homeSlot = nullptr; // empty host for HomeContent; not HomeContent itself
		EmbedContainer* m_embed = nullptr;
		QVector<TabInfo> m_tabs;
		qint64 m_activeTabId = kHomeTabId;
		QList<qint64> m_activationHistory; // MRU: most recently activated first
		QList<TabButton*> m_tabButtons;
		bool m_forceClosing = false;
		qint64 m_yieldDragTabId = 0;
		QVector<qint64> m_yieldOrder;
		bool m_stripDragLayoutActive = false;
		int m_dragTabWidth = 0;
		int m_stripDragOriginX = 0;
		QHash<qint64, QPropertyAnimation*> m_tabSlideAnims;
		bool m_embedResyncPending = false;
		bool m_renderHealPending = false;
		// NC hit-test rect cache, window-local logical coordinates (caption_hit_win.cpp).
		QRect m_hitTitleBarRect;
		QRect m_hitMinRect;
		QRect m_hitMaxRect;
		QRect m_hitCloseRect;
		QVector<QRect> m_hitInteractiveRects;
		int m_hitBandThickness = 0;
		bool m_hitCacheRefreshScheduled = false;
		// NC button visual state: -1 none, 0 min, 1 max, 2 close (caption_hit_win.cpp).
		int m_ncHoverButton = -1;
		int m_ncPressedButton = -1;
		bool m_ncTrackActive = false; // TrackMouseEvent(TME_NONCLIENT) armed
		// Hit part of the last WM_NCLBUTTONDOWN (caption_hit_win.cpp). The
		// double-click handler must classify by the press position, not a
		// fresh hit test: the first release may have maximized the window and
		// moved the buttons away from under the cursor.
		CaptionHitPart m_ncPressPart = CaptionHitPart::None;
	};
} // namespace mps::host

#endif // __MPS_HOST_SHELL_WINDOW_H__
