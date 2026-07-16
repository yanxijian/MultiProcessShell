#include "shell_window.hpp"

#include "shell_app.hpp"

#include <QCloseEvent>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>
#include <QWindow>
#include <QApplication>

namespace mps::host {
namespace {
qint64 g_nextShellId = 1;
const char* kTabMime = "application/x-mps-tab-id";
}

TabButton::TabButton(const TabInfo& info, QWidget* parent) : QFrame(parent), info_(info) {
  setObjectName(QStringLiteral("TabButton"));
  setFrameShape(QFrame::StyledPanel);
  setCursor(Qt::ArrowCursor);
  auto* lay = new QHBoxLayout(this);
  lay->setContentsMargins(10, 4, 6, 4);
  lay->setSpacing(6);
  title_ = new QLabel(info_.title, this);
  auto* closeBtn = new QPushButton(QStringLiteral("×"), this);
  closeBtn->setFixedSize(18, 18);
  closeBtn->setFlat(true);
  lay->addWidget(title_);
  lay->addWidget(closeBtn);
  connect(closeBtn, &QPushButton::clicked, this, [this] { emit closeRequested(info_.tabId); });
  const QColor accent = (info_.clientIndex % 2 == 0) ? QColor(200, 60, 60) : QColor(120, 70, 180);
  setStyleSheet(QStringLiteral(
      "#TabButton { border: 2px solid %1; border-radius: 4px; background: #f3f3f3; }"
      "#TabButton[active=\"true\"] { background: #ffffff; }")
                    .arg(accent.name()));
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
    dragging_ = false;
    emit activated(info_.tabId);
  }
  QFrame::mousePressEvent(event);
}

void TabButton::mouseMoveEvent(QMouseEvent* event) {
  if (!(event->buttons() & Qt::LeftButton)) {
    QFrame::mouseMoveEvent(event);
    return;
  }
  if ((event->pos() - dragStart_).manhattanLength() < QApplication::startDragDistance()) {
    QFrame::mouseMoveEvent(event);
    return;
  }
  if (!dragging_) {
    dragging_ = true;
    emit dragStarted(info_.tabId);
  }
  QFrame::mouseMoveEvent(event);
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
  titleBar_->setStyleSheet(QStringLiteral("#TitleBar { background: #e8e8e8; border-bottom: 1px solid #c0c0c0; }"));
  auto* titleLay = new QHBoxLayout(titleBar_);
  titleLay->setContentsMargins(8, 4, 8, 4);
  titleLay->setSpacing(6);
  tabRow_ = new QHBoxLayout();
  tabRow_->setSpacing(6);
  tabRow_->setContentsMargins(0, 0, 0, 0);
  titleLay->addLayout(tabRow_, 1);
  titleLay->addStretch(0);

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

  titleBar_->installEventFilter(this);

  auto* stack = new QStackedWidget(root);
  emptyPage_ = new QWidget(stack);
  auto* emptyLay = new QVBoxLayout(emptyPage_);
  createClientBtn_ = new QPushButton(QStringLiteral("创建 Client"), emptyPage_);
  createClientBtn_->setFixedSize(160, 40);
  emptyLay->addStretch();
  emptyLay->addWidget(createClientBtn_, 0, Qt::AlignCenter);
  emptyLay->addStretch();
  connect(createClientBtn_, &QPushButton::clicked, this, &ShellWindow::createClientClicked);

  embed_ = new EmbedContainer(stack);
  stack->addWidget(emptyPage_);
  stack->addWidget(embed_);

  rootLay->addWidget(titleBar_);
  rootLay->addWidget(stack, 1);

  showEmptyState(true);
  setAcceptDrops(true);
}

void ShellWindow::showEmptyState(bool empty) {
  auto* stack = qobject_cast<QStackedWidget*>(embed_->parent());
  if (!stack) {
    return;
  }
  stack->setCurrentWidget(empty ? emptyPage_ : static_cast<QWidget*>(embed_));
}

void ShellWindow::addTab(const TabInfo& info) {
  tabs_.push_back(info);
  setActiveTab(info.tabId);
  rebuildTabs();
  showEmptyState(false);
}

void ShellWindow::removeTab(qint64 tabId) {
  for (int i = 0; i < tabs_.size(); ++i) {
    if (tabs_[i].tabId == tabId) {
      tabs_.removeAt(i);
      break;
    }
  }
  if (activeTabId_ == tabId) {
    activeTabId_ = tabs_.isEmpty() ? 0 : tabs_.last().tabId;
  }
  rebuildTabs();
  syncEmbedToActive();
  showEmptyState(tabs_.isEmpty());
}

void ShellWindow::setActiveTab(qint64 tabId) {
  activeTabId_ = tabId;
  for (auto* b : tabButtons_) {
    b->setActive(b->info().tabId == tabId);
  }
  syncEmbedToActive();
  if (auto* t = findTab(tabId); t && t->session) {
    t->session->requestActivate(tabId);
  }
}

TabInfo* ShellWindow::findTab(qint64 tabId) {
  for (auto& t : tabs_) {
    if (t.tabId == tabId) {
      return &t;
    }
  }
  return nullptr;
}

void ShellWindow::syncEmbedToActive() {
  if (auto* t = findTab(activeTabId_)) {
    embed_->setForeignWindow(t->wid);
  } else {
    embed_->clearForeignWindow();
  }
}

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
    connect(btn, &TabButton::dragStarted, this, [this](qint64 tabId) {
      auto* mime = new QMimeData;
      mime->setData(QString::fromUtf8(kTabMime), QByteArray::number(tabId));
      auto* drag = new QDrag(this);
      drag->setMimeData(mime);
      if (auto* t = findTab(tabId); t && t->session) {
        t->session->setDragSuppress(true);
      }
      const auto drop = drag->exec(Qt::MoveAction);
      if (auto* t = findTab(tabId); t && t->session) {
        t->session->setDragSuppress(false);
      }
      if (drop == Qt::IgnoreAction) {
        emit tabTearOutRequested(tabId, QCursor::pos());
      }
    });
  }
  tabRow_->addStretch(1);
}

void ShellWindow::takeTabsFrom(ShellWindow* other, const QList<qint64>& tabIds) {
  for (qint64 id : tabIds) {
    for (int i = 0; i < other->tabs_.size(); ++i) {
      if (other->tabs_[i].tabId == id) {
        tabs_.push_back(other->tabs_[i]);
        other->tabs_.removeAt(i);
        break;
      }
    }
  }
  other->rebuildTabs();
  other->syncEmbedToActive();
  other->showEmptyState(other->tabs_.isEmpty());
  rebuildTabs();
  if (!tabs_.isEmpty()) {
    setActiveTab(tabs_.last().tabId);
  }
  showEmptyState(tabs_.isEmpty());
}

void ShellWindow::closeEvent(QCloseEvent* event) {
  const auto copy = tabs_;
  for (const auto& t : copy) {
    emit tabCloseRequested(t.tabId);
  }
  QMainWindow::closeEvent(event);
}

bool ShellWindow::eventFilter(QObject* watched, QEvent* event) {
  if (watched == titleBar_ && event->type() == QEvent::MouseButtonPress) {
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
