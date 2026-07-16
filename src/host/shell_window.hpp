#pragma once

#include "embed_container.hpp"
#include "tab_info.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
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
  void dragStarted(qint64 tabId);

protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;

private:
  TabInfo info_;
  QLabel* title_ = nullptr;
  QPoint dragStart_;
  bool dragging_ = false;
};

class ShellWindow final : public QMainWindow {
  Q_OBJECT
public:
  explicit ShellWindow(ShellApp* app, QWidget* parent = nullptr);

  [[nodiscard]] qint64 shellId() const { return shellId_; }
  void addTab(const TabInfo& info);
  void removeTab(qint64 tabId);
  void setActiveTab(qint64 tabId);
  [[nodiscard]] QVector<TabInfo> tabs() const { return tabs_; }
  [[nodiscard]] qint64 activeTabId() const { return activeTabId_; }
  [[nodiscard]] EmbedContainer* embed() { return embed_; }
  void showEmptyState(bool empty);
  void takeTabsFrom(ShellWindow* other, const QList<qint64>& tabIds);

signals:
  void createClientClicked();
  void tabCloseRequested(qint64 tabId);
  void tabActivated(qint64 tabId);
  void tabTearOutRequested(qint64 tabId, QPoint globalPos);
  void tabMergeRequested(qint64 tabId, ShellWindow* target, int insertIndex);

protected:
  void closeEvent(QCloseEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  void rebuildTabs();
  void syncEmbedToActive();
  TabInfo* findTab(qint64 tabId);

  ShellApp* app_ = nullptr;
  qint64 shellId_ = 0;
  QWidget* titleBar_ = nullptr;
  QHBoxLayout* tabRow_ = nullptr;
  QWidget* emptyPage_ = nullptr;
  QPushButton* createClientBtn_ = nullptr;
  EmbedContainer* embed_ = nullptr;
  QVector<TabInfo> tabs_;
  qint64 activeTabId_ = 0;
  QList<TabButton*> tabButtons_;
};

}  // namespace mps::host
