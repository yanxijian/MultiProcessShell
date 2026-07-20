#pragma once

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

namespace mps::host {

class ClientSession;
class ShellApp;

class TabButton final : public QFrame {
  Q_OBJECT
public:
  TabButton(const TabInfo& info, QWidget* parent = nullptr);
  [[nodiscard]] const TabInfo& info() const { return info_; }
  void setInfo(const TabInfo& info);
  void setActive(bool on);

signals:
  void closeRequested(qint64 tabId);
  void activated(qint64 tabId);
  /// localHotSpot: press position inside the tab (for Chrome-like grab offset).
  void dragStarted(qint64 tabId, QPoint localHotSpot);

protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  TabInfo info_;
  QLabel* title_ = nullptr;
  QPoint dragStart_;
  bool pressActive_ = false;
  bool dragging_ = false;
};

class ShellWindow final : public QMainWindow {
  Q_OBJECT
public:
  explicit ShellWindow(ShellApp* app, QWidget* parent = nullptr);

  [[nodiscard]] qint64 shellId() const { return shellId_; }
  void addTab(const TabInfo& info);
  void insertTab(const TabInfo& info, int insertIndex);
  void moveTab(qint64 tabId, int insertIndex);
  void removeTab(qint64 tabId);
  void setActiveTab(qint64 tabId);
  [[nodiscard]] QVector<TabInfo> tabs() const { return tabs_; }
  [[nodiscard]] qint64 activeTabId() const { return activeTabId_; }
  [[nodiscard]] int clientTabCount() const;
  [[nodiscard]] EmbedContainer* embed() { return embed_; }
  [[nodiscard]] QWidget* titleBarWidget() const { return titleBar_; }
  [[nodiscard]] bool isOverChrome(QPoint globalPos) const;
  /// Tab buttons + trailing strip (merge/reorder hot zone); excludes window buttons.
  [[nodiscard]] bool isOverTabDropZone(QPoint globalPos) const;
  [[nodiscard]] bool isNearTabDropZone(QPoint globalPos, int verticalSlop,
                                       int horizontalSlop) const;
  /// Min/max/close — not a valid drop target during tab drag.
  [[nodiscard]] bool isOverWindowChromeButtons(QPoint globalPos) const;
  [[nodiscard]] QRect tabStripGlobalRect() const;
  /// Local Y of tab buttons inside the title bar (centered, not hard-coded top).
  [[nodiscard]] int tabStripContentY() const;
  /// Top Y of the tab row in global coords (for locking reorder ghost vertically).
  [[nodiscard]] int tabRowTopGlobal() const;
  [[nodiscard]] bool isChromeDropTarget(const QObject* watched) const;
  [[nodiscard]] int tabInsertIndexAt(QPoint globalPos) const;
  void updateDropInsertIndicator(int insertIndex);
  void clearDropInsertIndicator();
  /// Live reorder/merge yield from cursor (model unchanged until drop).
  /// guestWidth > 0: drag tab is not in this shell (merge into target).
  /// hotSpotX: ghost grab offset; <0 means center.
  void previewTabYieldAtCursor(qint64 dragTabId, QPoint globalPos, int guestWidth = 0,
                               int hotSpotX = -1);
  /// Apply yieldOrder_ to the tab model (same-shell drop). Returns true if applied.
  bool commitTabYieldPreview();
  void clearTabYieldPreview();
  /// Chrome tear-out: siblings immediately claim the vacated strip slot (no gap).
  void collapseTornOutTabSlot(qint64 dragTabId);
  [[nodiscard]] bool hasTabYieldPreview() const { return yieldDragTabId_ != 0; }
  /// Insert index of the dragged tab in the live yield order (-1 if none).
  [[nodiscard]] int yieldInsertIndex() const;
  /// Global rect of the drag slot (for cancel snap-back).
  [[nodiscard]] QRect tabDragSlotGlobalRect(qint64 tabId) const;
  /// Stop tracking active HWND without Hide (tear-out/merge handoff).
  void releaseEmbedOwnershipForTab(qint64 tabId);
  /// Chrome-like: keep layout slot but make the dragged tab invisible.
  void setTabDragHidden(qint64 tabId, bool hidden);
  [[nodiscard]] qint64 previousActivationTarget(qint64 closingTabId) const;
  [[nodiscard]] QPixmap grabTabButton(qint64 tabId) const;
  /// Logical size of a tab button (for drag ghost hotspot on high-DPI).
  [[nodiscard]] QSize tabButtonSize(qint64 tabId) const;
  void installChromeDropFilter(QObject* filter);
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
  void reinstallChromeDropTargets();
  void scheduleEmbedResync();
  void ensureStripDragLayout(qint64 hideTabId, int guestWidth = 0);
  void animateTabGeometry(TabButton* btn, const QRect& target);
  void stopTabSlideAnimations();
  TabInfo* findTab(qint64 tabId);
  [[nodiscard]] const TabInfo* findTab(qint64 tabId) const;

  ShellApp* app_ = nullptr;
  QObject* chromeDropFilter_ = nullptr;
  qint64 shellId_ = 0;
  QWidget* titleBar_ = nullptr;
  QWidget* tabDropTrail_ = nullptr;  // trailing strip: drop-to-append + system-move
  QWidget* dropIndicator_ = nullptr;
  QPushButton* minBtn_ = nullptr;
  QPushButton* maxBtn_ = nullptr;
  QPushButton* closeBtn_ = nullptr;
  QHBoxLayout* tabRow_ = nullptr;
  QStackedWidget* stack_ = nullptr;
  QWidget* emptyPage_ = nullptr;
  QPushButton* createClientBtn_ = nullptr;
  EmbedContainer* embed_ = nullptr;
  QVector<TabInfo> tabs_;
  qint64 activeTabId_ = kHomeTabId;
  QList<qint64> activationHistory_;  // MRU: most recently activated first
  QList<TabButton*> tabButtons_;
  bool forceClosing_ = false;
  qint64 yieldDragTabId_ = 0;
  QVector<qint64> yieldOrder_;
  bool stripDragLayoutActive_ = false;
  int dragTabWidth_ = 0;
  int stripDragOriginX_ = 0;
  QHash<qint64, QPropertyAnimation*> tabSlideAnims_;
};

}  // namespace mps::host
