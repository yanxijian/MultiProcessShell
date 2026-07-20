#include "shell_window.hpp"

#include "shell_app.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

namespace mps::host {
namespace {
qint64 g_nextShellId = 1;
const char* kTabMime = "application/x-mps-tab-id";

QPixmap makeTabDragCursor() {
  QPixmap pm(28, 28);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(QColor(40, 40, 40), 1.2));
  p.setBrush(QColor(245, 245, 245, 230));
  p.drawRoundedRect(QRectF(3, 8, 18, 12), 3, 3);
  p.setBrush(QColor(80, 80, 80));
  p.setPen(Qt::NoPen);
  p.drawEllipse(QPointF(20, 20), 5, 5);
  return pm;
}
}  // namespace

TabButton::TabButton(const TabInfo& info, QWidget* parent) : QFrame(parent), info_(info) {
  setObjectName(QStringLiteral("TabButton"));
  setFrameShape(QFrame::StyledPanel);
  setCursor(info_.isHome ? Qt::ArrowCursor : Qt::OpenHandCursor);
  setAttribute(Qt::WA_Hover, true);
  auto* lay = new QHBoxLayout(this);
  lay->setContentsMargins(10, 4, 6, 4);
  lay->setSpacing(6);
  title_ = new QLabel(info_.title, this);
  title_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  lay->addWidget(title_);
  if (!info_.isHome) {
    auto* closeBtn = new QPushButton(QStringLiteral("×"), this);
    closeBtn->setFixedSize(18, 18);
    closeBtn->setFlat(true);
    closeBtn->setCursor(Qt::ArrowCursor);
    lay->addWidget(closeBtn);
    connect(closeBtn, &QPushButton::clicked, this, [this] { emit closeRequested(info_.tabId); });
  }
  if (info_.isHome) {
    setStyleSheet(QStringLiteral(
        "#TabButton { border: 2px solid #5a5a5a; border-radius: 4px; background: #f3f3f3; }"
        "#TabButton[active=\"true\"] { background: #ffffff; }"));
  } else {
    const QColor accent =
        (info_.clientIndex % 2 == 0) ? QColor(200, 60, 60) : QColor(120, 70, 180);
    setStyleSheet(QStringLiteral(
                      "#TabButton { border: 2px solid %1; border-radius: 4px; background: #f3f3f3; }"
                      "#TabButton[active=\"true\"] { background: #ffffff; }")
                      .arg(accent.name()));
  }
}

void TabButton::setInfo(const TabInfo& info) {
  info_ = info;
  title_->setText(info_.title);
}

void TabButton::setActive(bool on) {
  setProperty("active", on);
  style()->unpolish(this);
  style()->polish(this);
}

void TabButton::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    dragStart_ = event->pos();
    pressActive_ = true;
    dragging_ = false;
    emit activated(info_.tabId);
    event->accept();
    return;
  }
  QFrame::mousePressEvent(event);
}

void TabButton::mouseMoveEvent(QMouseEvent* event) {
  if (!pressActive_ || info_.isHome || !(event->buttons() & Qt::LeftButton)) {
    QFrame::mouseMoveEvent(event);
    return;
  }
  if ((event->pos() - dragStart_).manhattanLength() < QApplication::startDragDistance()) {
    event->accept();
    return;
  }
  if (!dragging_) {
    dragging_ = true;
    setCursor(Qt::ClosedHandCursor);
    emit dragStarted(info_.tabId);
  }
  event->accept();
}

void TabButton::mouseReleaseEvent(QMouseEvent* event) {
  pressActive_ = false;
  dragging_ = false;
  setCursor(info_.isHome ? Qt::ArrowCursor : Qt::OpenHandCursor);
  QFrame::mouseReleaseEvent(event);
}

ShellWindow::ShellWindow(ShellApp* app, QWidget* parent)
    : QMainWindow(parent), app_(app), shellId_(g_nextShellId++) {
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
  setMinimumSize(720, 480);
  resize(960, 640);
  setWindowTitle(QStringLiteral("MultiProcessShell Demo"));

  auto* root = new QWidget(this);
  setCentralWidget(root);
  auto* rootLay = new QVBoxLayout(root);
  rootLay->setContentsMargins(0, 0, 0, 0);
  rootLay->setSpacing(0);

  titleBar_ = new QWidget(root);
  titleBar_->setObjectName(QStringLiteral("TitleBar"));
  titleBar_->setFixedHeight(40);
  titleBar_->setStyleSheet(
      QStringLiteral("#TitleBar { background: #e8e8e8; border-bottom: 1px solid #c0c0c0; }"));
  auto* titleLay = new QHBoxLayout(titleBar_);
  titleLay->setContentsMargins(8, 4, 8, 4);
  titleLay->setSpacing(6);

  tabRow_ = new QHBoxLayout();
  tabRow_->setSpacing(6);
  tabRow_->setContentsMargins(0, 0, 0, 0);
  titleLay->addLayout(tabRow_, 0);

  // Blank caption strip: ONLY this area starts system-move (not the tabs).
  captionDrag_ = new QWidget(titleBar_);
  captionDrag_->setObjectName(QStringLiteral("CaptionDrag"));
  captionDrag_->setMinimumWidth(48);
  captionDrag_->setCursor(Qt::SizeAllCursor);
  captionDrag_->setStyleSheet(QStringLiteral("#CaptionDrag { background: transparent; }"));
  titleLay->addWidget(captionDrag_, 1);
  captionDrag_->installEventFilter(this);

  auto* minBtn = new QPushButton(QStringLiteral("—"), titleBar_);
  auto* maxBtn = new QPushButton(QStringLiteral("□"), titleBar_);
  auto* closeBtn = new QPushButton(QStringLiteral("×"), titleBar_);
  for (auto* b : {minBtn, maxBtn, closeBtn}) {
    b->setFixedSize(28, 24);
    b->setFlat(true);
    titleLay->addWidget(b);
  }
  connect(minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
  connect(maxBtn, &QPushButton::clicked, this, [this] {
    isMaximized() ? showNormal() : showMaximized();
  });
  connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);

  stack_ = new QStackedWidget(root);
  emptyPage_ = new QWidget(stack_);
  auto* emptyLay = new QVBoxLayout(emptyPage_);
  createClientBtn_ = new QPushButton(QStringLiteral("创建 Client"), emptyPage_);
  createClientBtn_->setFixedSize(160, 40);
  emptyLay->addStretch();
  emptyLay->addWidget(createClientBtn_, 0, Qt::AlignCenter);
  emptyLay->addStretch();
  connect(createClientBtn_, &QPushButton::clicked, this, &ShellWindow::createClientClicked);

  embed_ = new EmbedContainer(stack_);
  stack_->addWidget(emptyPage_);
  stack_->addWidget(embed_);

  rootLay->addWidget(titleBar_);
  rootLay->addWidget(stack_, 1);

  tabs_.push_back(TabInfo::makeHome());
  activeTabId_ = kHomeTabId;
  activationHistory_ = {kHomeTabId};
  rebuildTabs();
  syncWorkspace();
  setAcceptDrops(true);
}

void ShellWindow::showEmptyState(bool empty) {
  Q_UNUSED(empty);
  syncWorkspace();
}

void ShellWindow::syncWorkspace() {
  if (!stack_) {
    return;
  }
  const TabInfo* active = findTab(activeTabId_);
  const bool showHome = !active || active->isHome || active->wid == 0;
  if (showHome) {
    embed_->clearForeignWindow(true);
    stack_->setCurrentWidget(emptyPage_);
  } else {
    stack_->setCurrentWidget(embed_);
    embed_->setForeignWindow(active->wid);
    scheduleEmbedResync();
  }
}

void ShellWindow::scheduleEmbedResync() {
  QTimer::singleShot(0, this, [this] {
    if (auto* t = findTab(activeTabId_); t && !t->isHome && t->wid) {
      embed_->setForeignWindow(t->wid);
      embed_->resyncForeignWindow();
    }
  });
}

void ShellWindow::releaseEmbedOwnershipForTab(qint64 tabId) {
  if (activeTabId_ != tabId || !embed_) {
    return;
  }
  embed_->releaseForeignWindow();
}

void ShellWindow::updateDropInsertIndicator(int insertIndex) {
  if (!titleBar_ || tabButtons_.isEmpty()) {
    return;
  }
  insertIndex = qBound(1, insertIndex, tabs_.size());
  if (!dropIndicator_) {
    dropIndicator_ = new QWidget(titleBar_);
    dropIndicator_->setObjectName(QStringLiteral("DropInsertIndicator"));
    dropIndicator_->setFixedWidth(3);
    dropIndicator_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    dropIndicator_->setStyleSheet(
        QStringLiteral("#DropInsertIndicator { background: #2d6cdf; border-radius: 1px; }"));
  }

  int x = 8;
  if (insertIndex < tabButtons_.size()) {
    auto* btn = tabButtons_[insertIndex];
    if (btn) {
      x = btn->geometry().left();
    }
  } else if (!tabButtons_.isEmpty()) {
    auto* last = tabButtons_.last();
    if (last) {
      x = last->geometry().right() + 2;
    }
  }
  const int y = 4;
  const int h = qMax(8, titleBar_->height() - 8);
  dropIndicator_->setGeometry(x - 1, y, 3, h);
  dropIndicator_->show();
  dropIndicator_->raise();
}

void ShellWindow::clearDropInsertIndicator() {
  if (dropIndicator_) {
    dropIndicator_->hide();
  }
}

int ShellWindow::clientTabCount() const {
  int n = 0;
  for (const auto& t : tabs_) {
    if (!t.isHome) {
      ++n;
    }
  }
  return n;
}

bool ShellWindow::isOverChrome(QPoint globalPos) const {
  if (!titleBar_) {
    return false;
  }
  const QRect r(titleBar_->mapToGlobal(QPoint(0, 0)), titleBar_->size());
  return r.contains(globalPos);
}

bool ShellWindow::isChromeDropTarget(const QObject* watched) const {
  if (!watched || !titleBar_) {
    return false;
  }
  if (watched == titleBar_ || watched == captionDrag_) {
    return true;
  }
  const auto* w = qobject_cast<const QWidget*>(watched);
  return w && titleBar_->isAncestorOf(w);
}

void ShellWindow::installChromeDropFilter(QObject* filter) {
  chromeDropFilter_ = filter;
  setAcceptDrops(false);
  reinstallChromeDropTargets();
}

void ShellWindow::reinstallChromeDropTargets() {
  if (!chromeDropFilter_ || !titleBar_) {
    return;
  }
  titleBar_->setAcceptDrops(true);
  titleBar_->installEventFilter(chromeDropFilter_);
  if (captionDrag_) {
    captionDrag_->setAcceptDrops(true);
    captionDrag_->installEventFilter(chromeDropFilter_);
  }
  for (auto* btn : tabButtons_) {
    if (!btn) {
      continue;
    }
    btn->setAcceptDrops(true);
    btn->installEventFilter(chromeDropFilter_);
  }
}

void ShellWindow::addTab(const TabInfo& info) {
  insertTab(info, tabs_.size());
}

void ShellWindow::insertTab(const TabInfo& info, int insertIndex) {
  if (info.isHome) {
    return;
  }
  // Home stays at index 0; client tabs occupy [1, size].
  insertIndex = qBound(1, insertIndex, tabs_.size());
  tabs_.insert(insertIndex, info);
  setActiveTab(info.tabId);
  rebuildTabs();
}

void ShellWindow::moveTab(qint64 tabId, int insertIndex) {
  if (tabId == kHomeTabId) {
    return;
  }
  int from = -1;
  for (int i = 0; i < tabs_.size(); ++i) {
    if (tabs_[i].tabId == tabId) {
      from = i;
      break;
    }
  }
  if (from < 0 || tabs_[from].isHome) {
    return;
  }
  insertIndex = qBound(1, insertIndex, tabs_.size());
  if (insertIndex == from || insertIndex == from + 1) {
    return;  // same slot (before/after self)
  }
  const TabInfo moved = tabs_.takeAt(from);
  if (insertIndex > from) {
    --insertIndex;
  }
  insertIndex = qBound(1, insertIndex, tabs_.size());
  tabs_.insert(insertIndex, moved);
  rebuildTabs();
  setActiveTab(tabId);
}

int ShellWindow::tabInsertIndexAt(QPoint globalPos) const {
  // Default: append after all tabs.
  int insert = tabs_.size();
  for (int i = 0; i < tabButtons_.size(); ++i) {
    auto* btn = tabButtons_[i];
    if (!btn) {
      continue;
    }
    const QRect r(btn->mapToGlobal(QPoint(0, 0)), btn->size());
    if (!r.contains(globalPos)) {
      continue;
    }
    if (btn->info().isHome) {
      // Never place a client tab before Home.
      return 1;
    }
    int idx = -1;
    for (int t = 0; t < tabs_.size(); ++t) {
      if (tabs_[t].tabId == btn->info().tabId) {
        idx = t;
        break;
      }
    }
    if (idx < 0) {
      break;
    }
    return (globalPos.x() < r.center().x()) ? idx : (idx + 1);
  }
  return insert;
}

void ShellWindow::removeTab(qint64 tabId) {
  if (tabId == kHomeTabId) {
    return;
  }
  const bool wasActive = (activeTabId_ == tabId);
  for (int i = 0; i < tabs_.size(); ++i) {
    if (tabs_[i].tabId == tabId) {
      tabs_.removeAt(i);
      break;
    }
  }
  activationHistory_.removeAll(tabId);
  if (wasActive) {
    const qint64 next = previousActivationTarget(tabId);
    rebuildTabs();
    setActiveTab(next);
    return;
  }
  rebuildTabs();
  syncWorkspace();
}

void ShellWindow::setActiveTab(qint64 tabId) {
  if (!findTab(tabId)) {
    tabId = kHomeTabId;
  }
  pushActivationHistory(tabId);
  activeTabId_ = tabId;
  for (auto* b : tabButtons_) {
    b->setActive(b->info().tabId == tabId);
  }
  syncWorkspace();
  if (auto* t = findTab(tabId); t && t->session && !t->isHome) {
    t->session->requestActivate(tabId);
    raise();
    activateWindow();
  }
}

void ShellWindow::pushActivationHistory(qint64 tabId) {
  activationHistory_.removeAll(tabId);
  activationHistory_.prepend(tabId);
}

qint64 ShellWindow::previousActivationTarget(qint64 /*closingTabId*/) const {
  for (qint64 id : activationHistory_) {
    if (findTab(id)) {
      return id;
    }
  }
  for (const auto& t : tabs_) {
    if (!t.isHome) {
      return t.tabId;
    }
  }
  return kHomeTabId;
}

TabInfo* ShellWindow::findTab(qint64 tabId) {
  for (auto& t : tabs_) {
    if (t.tabId == tabId) {
      return &t;
    }
  }
  return nullptr;
}

const TabInfo* ShellWindow::findTab(qint64 tabId) const {
  for (const auto& t : tabs_) {
    if (t.tabId == tabId) {
      return &t;
    }
  }
  return nullptr;
}

void ShellWindow::syncEmbedToActive() { syncWorkspace(); }

void ShellWindow::rebuildTabs() {
  while (QLayoutItem* item = tabRow_->takeAt(0)) {
    if (auto* w = item->widget()) {
      w->deleteLater();
    }
    delete item;
  }
  tabButtons_.clear();
  for (const auto& t : tabs_) {
    auto* btn = new TabButton(t, titleBar_);
    tabButtons_.push_back(btn);
    tabRow_->addWidget(btn);
    btn->setActive(t.tabId == activeTabId_);
    connect(btn, &TabButton::activated, this, &ShellWindow::tabActivated);
    connect(btn, &TabButton::closeRequested, this, &ShellWindow::tabCloseRequested);
    if (!t.isHome) {
      connect(btn, &TabButton::dragStarted, this, [this](qint64 tabId) {
        auto* mime = new QMimeData;
        mime->setData(QString::fromUtf8(kTabMime), QByteArray::number(tabId));
        auto* drag = new QDrag(this);
        drag->setMimeData(mime);
        if (auto* info = findTab(tabId); info && info->session) {
          info->session->setDragSuppress(true);
        }
        // Drag pixmap from the tab chrome (simple visual feedback).
        if (auto* btnSender = qobject_cast<TabButton*>(sender())) {
          drag->setPixmap(btnSender->grab());
          drag->setHotSpot(QPoint(btnSender->width() / 2, btnSender->height() / 2));
        }
        const QPixmap cursorPm = makeTabDragCursor();
        drag->setDragCursor(cursorPm, Qt::MoveAction);
        drag->setDragCursor(cursorPm, Qt::CopyAction);
        drag->setDragCursor(cursorPm, Qt::IgnoreAction);
        const auto drop = drag->exec(Qt::MoveAction);
        emit dropIndicatorsClearRequested();
        clearDropInsertIndicator();
        if (auto* info = findTab(tabId); info && info->session) {
          info->session->setDragSuppress(false);
        }
        // Drop accepted on a shell chrome → merge/stay already handled.
        // IgnoreAction: released outside, or over client content (not chrome) → tear out.
        if (drop == Qt::IgnoreAction) {
          emit tabTearOutRequested(tabId, QCursor::pos());
        }
      });
    }
  }
  reinstallChromeDropTargets();
}

void ShellWindow::takeTabsFrom(ShellWindow* other, const QList<qint64>& tabIds) {
  for (qint64 id : tabIds) {
    if (id == kHomeTabId) {
      continue;
    }
    for (int i = 0; i < other->tabs_.size(); ++i) {
      if (other->tabs_[i].tabId == id) {
        tabs_.push_back(other->tabs_[i]);
        other->tabs_.removeAt(i);
        other->activationHistory_.removeAll(id);
        break;
      }
    }
  }
  if (!other->findTab(other->activeTabId_)) {
    other->setActiveTab(other->previousActivationTarget(0));
  } else {
    other->rebuildTabs();
    other->syncWorkspace();
  }
  rebuildTabs();
  if (clientTabCount() > 0) {
    setActiveTab(tabs_.last().tabId);
  } else {
    setActiveTab(kHomeTabId);
  }
}

void ShellWindow::forceClose() {
  forceClosing_ = true;
  clearDropInsertIndicator();
  if (embed_) {
    embed_->releaseForeignWindow();
  }
  close();
}

void ShellWindow::closeEvent(QCloseEvent* event) {
  if (forceClosing_) {
    QMainWindow::closeEvent(event);
    return;
  }
  // Route through ShellApp so tabs/sessions/shells_ stay consistent.
  event->ignore();
  emit shellCloseRequested(this);
}

bool ShellWindow::eventFilter(QObject* watched, QEvent* event) {
  // System-move is bound to the blank caption strip only — never to Tab buttons.
  if (watched == captionDrag_ && event->type() == QEvent::MouseButtonPress) {
    auto* me = static_cast<QMouseEvent*>(event);
    if (me->button() == Qt::LeftButton) {
      winId();
      if (windowHandle()) {
        windowHandle()->startSystemMove();
      }
      return true;
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

}  // namespace mps::host
