#include "shell_window.hpp"

#include "shell_app.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

namespace mps::host {
namespace {
qint64 g_nextShellId = 1;
const char* kTabMime = "application/x-mps-tab-id";
}

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
    embed_->clearForeignWindow();
    stack_->setCurrentWidget(emptyPage_);
  } else {
    stack_->setCurrentWidget(embed_);
    embed_->setForeignWindow(active->wid);
    QTimer::singleShot(0, this, [this] {
      if (auto* t = findTab(activeTabId_); t && !t->isHome && t->wid) {
        embed_->setForeignWindow(t->wid);
      }
    });
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

void ShellWindow::addTab(const TabInfo& info) {
  if (info.isHome) {
    return;
  }
  tabs_.push_back(info);
  setActiveTab(info.tabId);
  rebuildTabs();
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
        const auto drop = drag->exec(Qt::MoveAction);
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

void ShellWindow::closeEvent(QCloseEvent* event) {
  const auto copy = tabs_;
  for (const auto& t : copy) {
    if (!t.isHome) {
      emit tabCloseRequested(t.tabId);
    }
  }
  QMainWindow::closeEvent(event);
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
