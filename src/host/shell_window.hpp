#ifndef __MPS_HOST_SHELL_WINDOW_H__
#define __MPS_HOST_SHELL_WINDOW_H__

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

	signals:
		void closeRequested(qint64 tabId);
		void activated(qint64 tabId);
		/// localHotSpot: press position inside the tab (for grab offset).
		void dragStarted(qint64 tabId, QPoint localHotSpot);

	protected:
		void mousePressEvent(QMouseEvent* event) override;
		void mouseMoveEvent(QMouseEvent* event) override;
		void mouseReleaseEvent(QMouseEvent* event) override;
		bool eventFilter(QObject* watched, QEvent* event) override;

	private:
		TabInfo m_info;
		QLabel* m_title = nullptr;
		QPoint m_dragStart;
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
		[[nodiscard]] QVector<TabInfo> tabs() const
		{
			return m_tabs;
		}
		[[nodiscard]] qint64 activeTabId() const
		{
			return m_activeTabId;
		}
		[[nodiscard]] int clientTabCount() const;
		[[nodiscard]] EmbedContainer* embed()
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
		void clearTabYieldPreview();
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
		void releaseEmbedOwnershipForTab(qint64 tabId);
		/// Keep layout slot but make the dragged tab invisible.
		void setTabDragHidden(qint64 tabId, bool hidden);
		[[nodiscard]] qint64 previousActivationTarget(qint64 closingTabId) const;
		[[nodiscard]] QPixmap grabTabButton(qint64 tabId) const;
		/// Logical size of a tab button (for drag ghost hotspot on high-DPI).
		[[nodiscard]] QSize tabButtonSize(qint64 tabId) const;
		void installStripDropFilter(QObject* filter);
		void showEmptyState(bool empty);
		void takeTabsFrom(ShellWindow* other, const QList<qint64>& tabIds);
		/// Close without emitting shellCloseRequested (app-driven teardown).
		void forceClose();

	signals:
		void createClientClicked();
		void tabCloseRequested(qint64 tabId);
		void tabActivated(qint64 tabId);
		void tabTearOutRequested(qint64 tabId, QRect suggestedGeometry);
		void tabMergeRequested(qint64 tabId, ShellWindow* target, int insertIndex);
		void shellCloseRequested(ShellWindow* self);
		void dropIndicatorsClearRequested();

	protected:
		void closeEvent(QCloseEvent* event) override;
		bool eventFilter(QObject* watched, QEvent* event) override;

	private:
		void rebuildTabs();
		void syncEmbedToActive();
		void syncWorkspace();
		void pushActivationHistory(qint64 tabId);
		void reinstallStripDropTargets();
		void scheduleEmbedResync();
		void ensureStripDragLayout(qint64 hideTabId, int guestWidth = 0);
		void animateTabGeometry(TabButton* btn, const QRect& target);
		void stopTabSlideAnimations();
		TabInfo* findTab(qint64 tabId);
		[[nodiscard]] const TabInfo* findTab(qint64 tabId) const;

		ShellApp* m_app = nullptr;
		QObject* m_stripDropFilter = nullptr;
		qint64 m_shellId = 0;
		QWidget* m_titleBar = nullptr;
		QWidget* m_tabDropTrail = nullptr; // trailing strip: drop-to-append + system-move
		QWidget* m_dropIndicator = nullptr;
		QPushButton* m_minBtn = nullptr;
		QPushButton* m_maxBtn = nullptr;
		QPushButton* m_closeBtn = nullptr;
		QHBoxLayout* m_tabRow = nullptr;
		QStackedWidget* m_stack = nullptr;
		QWidget* m_emptyPage = nullptr;
		QPushButton* m_createClientBtn = nullptr;
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
	};
} // namespace mps::host

#endif  // __MPS_HOST_SHELL_WINDOW_H__
