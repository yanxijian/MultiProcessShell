#include "shell_app.hpp"

#include <QLocalSocket>
#include <QUuid>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <algorithm>

namespace mps::host {
namespace {
const char* kTabMime = "application/x-mps-tab-id";
}

ShellApp::ShellApp(QString clientExe, QObject* parent)
    : QObject(parent), clientExe_(std::move(clientExe)) {
  token_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
  endpoint_ = QStringLiteral("mps-demo-%1").arg(token_);
  server_ = new QLocalServer(this);
  QLocalServer::removeServer(endpoint_);
  if (!server_->listen(endpoint_)) {
    qWarning("Failed to listen on %s", qPrintable(endpoint_));
  }
  connect(server_, &QLocalServer::newConnection, this, &ShellApp::onNewConnection);
}

ShellWindow* ShellApp::createShell(QPoint pos) {
  auto shell = std::make_unique<ShellWindow>(this);
  auto* raw = shell.get();
  bindShell(raw);
  if (!pos.isNull()) {
    raw->move(pos);
  }
  raw->show();
  shells_.push_back(std::move(shell));
  return raw;
}

void ShellApp::bindShell(ShellWindow* shell) {
  connect(shell, &ShellWindow::createClientClicked, this, [this, shell] {
    createClientOn(shell);
  });
  connect(shell, &ShellWindow::tabCloseRequested, this, [this](qint64 tabId) {
    closeTab(tabId);
  });
  connect(shell, &ShellWindow::tabActivated, this, [this, shell](qint64 tabId) {
    activateTab(shell, tabId);
  });
  connect(shell, &ShellWindow::tabTearOutRequested, this,
          [this, shell](qint64 tabId, QPoint pos) { tearOutTab(shell, tabId, pos); });
  connect(shell, &ShellWindow::shellCloseRequested, this, &ShellApp::closeShell);
  connect(shell, &ShellWindow::dropIndicatorsClearRequested, this,
          &ShellApp::clearAllDropIndicators);

  shell->installChromeDropFilter(this);
}

void ShellApp::clearAllDropIndicators() {
  for (auto& shell : shells_) {
    if (shell) {
      shell->clearDropInsertIndicator();
    }
  }
}

ShellWindow* ShellApp::shellFromChromeTarget(QObject* watched) const {
  if (auto* shell = qobject_cast<ShellWindow*>(watched)) {
    return shell;
  }
  for (const auto& shell : shells_) {
    if (shell && shell->isChromeDropTarget(watched)) {
      return shell.get();
    }
  }
  return nullptr;
}

bool ShellApp::eventFilter(QObject* watched, QEvent* event) {
  // Caption strip still uses ShellWindow's own filter for system-move.
  if (event->type() == QEvent::MouseButtonPress) {
    return QObject::eventFilter(watched, event);
  }

  auto* shell = shellFromChromeTarget(watched);
  if (!shell) {
    return QObject::eventFilter(watched, event);
  }

  if (event->type() == QEvent::DragLeave) {
    shell->clearDropInsertIndicator();
    return false;
  }

  if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
    auto* de = static_cast<QDragMoveEvent*>(event);
    if (!de->mimeData()->hasFormat(QString::fromUtf8(kTabMime))) {
      return false;
    }
    QPoint dropGlobal = QCursor::pos();
    if (auto* w = qobject_cast<QWidget*>(watched)) {
      dropGlobal = w->mapToGlobal(de->position().toPoint());
    }
    // Only one shell shows an indicator at a time.
    for (auto& s : shells_) {
      if (s && s.get() != shell) {
        s->clearDropInsertIndicator();
      }
    }
    shell->updateDropInsertIndicator(shell->tabInsertIndexAt(dropGlobal));
    de->acceptProposedAction();
    return true;
  }
  if (event->type() == QEvent::Drop) {
    auto* de = static_cast<QDropEvent*>(event);
    if (!de->mimeData()->hasFormat(QString::fromUtf8(kTabMime))) {
      return false;
    }
    const qint64 tabId = de->mimeData()->data(QString::fromUtf8(kTabMime)).toLongLong();
    QPoint dropGlobal = QCursor::pos();
    if (auto* w = qobject_cast<QWidget*>(watched)) {
      dropGlobal = w->mapToGlobal(de->position().toPoint());
    }
    const int insertIndex = shell->tabInsertIndexAt(dropGlobal);
    clearAllDropIndicators();
    auto* source = shellForTab(tabId);
    if (!source) {
      de->acceptProposedAction();
      return true;
    }
    if (source == shell) {
      // Same top-level window: reorder tab + bound client.
      shell->moveTab(tabId, insertIndex);
      de->acceptProposedAction();
      return true;
    }
    mergeTab(tabId, shell, insertIndex);
    de->acceptProposedAction();
    return true;
  }
  return QObject::eventFilter(watched, event);
}

void ShellApp::createClientOn(ShellWindow* shell) {
  const int clientIndex = nextClientIndex_++;
  nextWindowIndex_[clientIndex] = 1;
  auto session = std::make_unique<ClientSession>(clientIndex, endpoint_, this);
  auto* raw = session.get();
  pendingFirstShell_.insert(raw, shell);
  connect(raw, &ClientSession::sessionHelloOk, this, &ShellApp::onSessionReady);
  connect(raw, &ClientSession::subWindowAdded, this, &ShellApp::onSubWindowAdded);
  connect(raw, &ClientSession::subWindowRemoved, this, &ShellApp::onSubWindowRemoved);
  connect(raw, &ClientSession::sessionDead, this, &ShellApp::onSessionDead);
  connect(raw, &ClientSession::invokeNewWindow, this,
          [this](ClientSession* session, qint64 sourceTabId) {
            const int m = nextWindowIndex_[session->clientIndex()]++;
            const qint64 tabId = nextTabId_++;
            const QString title = makeTitle(session->clientIndex(), m);
            // Prefer the shell that hosts the page where the user clicked.
            ShellWindow* shell = shellForTab(sourceTabId);
            if (!shell) {
              // Fallback: any shell that already hosts this session (prefer last match).
              for (auto& s : shells_) {
                for (const auto& t : s->tabs()) {
                  if (t.session == session) {
                    shell = s.get();
                  }
                }
              }
            }
            if (!shell && !shells_.empty()) {
              shell = shells_.back().get();
            }
            if (shell) {
              pendingFirstShell_.insert(session, shell);
            }
            session->requestCreateSubWindow(tabId, title);
          });
  raw->startClientProcess(clientExe_, token_);
  sessions_.push_back(std::move(session));
}

void ShellApp::onNewConnection() {
  while (server_->hasPendingConnections()) {
    auto* sock = server_->nextPendingConnection();
    // Attach to the newest session still without a socket.
    ClientSession* target = nullptr;
    for (auto it = sessions_.rbegin(); it != sessions_.rend(); ++it) {
      if (!(*it)->channel()) {
        target = it->get();
        break;
      }
    }
    if (!target) {
      sock->abort();
      sock->deleteLater();
      continue;
    }
    target->attachSocket(sock);
  }
}

void ShellApp::onSessionReady(ClientSession* session) {
  auto* shell = pendingFirstShell_.value(session, nullptr);
  if (!shell) {
    return;
  }
  const int m = nextWindowIndex_[session->clientIndex()]++;
  const qint64 tabId = nextTabId_++;
  const QString title = makeTitle(session->clientIndex(), m);
  session->requestCreateSubWindow(tabId, title);
}

void ShellApp::onSubWindowAdded(ClientSession* session, qint64 tabId, QString title,
                                quintptr wid) {
  ShellWindow* shell = pendingFirstShell_.take(session);
  if (!shell) {
    // Additional window without pending: prefer shell that already hosts this session
    // (last match — closer to tear-out target than shells_.front()).
    for (auto& s : shells_) {
      for (const auto& t : s->tabs()) {
        if (t.session == session) {
          shell = s.get();
        }
      }
    }
  }
  if (!shell && !shells_.empty()) {
    shell = shells_.back().get();
  }
  if (!shell) {
    return;
  }
  TabInfo info;
  info.pageId = session->pageId();
  info.tabId = tabId;
  info.clientIndex = session->clientIndex();
  info.title = title;
  info.wid = wid;
  info.session = session;
  // parse window index from title ClientN-WindowM
  const auto parts = title.split(QLatin1Char('-'));
  if (parts.size() == 2 && parts[1].startsWith(QStringLiteral("Window"))) {
    info.windowIndex = parts[1].mid(6).toInt();
  }
  tabToShell_.insert(tabId, shell);
  shell->addTab(info);
  session->notifyReattachment(shell->shellId());
}

void ShellApp::onSubWindowRemoved(ClientSession* session, qint64 tabId) {
  Q_UNUSED(session);
  if (auto* shell = tabToShell_.take(tabId)) {
    shell->removeTab(tabId);
    destroyShellIfEmpty(shell);
  }
}

void ShellApp::onSessionDead(ClientSession* session) {
  if (!session) {
    return;
  }
  // Disconnect further death signals — process finished + socket disconnect both fire.
  session->disconnect(this);

  QList<qint64> tabs;
  for (auto& shell : shells_) {
    if (!shell) {
      continue;
    }
    const auto copy = shell->tabs();
    for (const auto& t : copy) {
      if (t.session == session) {
        tabs.push_back(t.tabId);
      }
    }
  }
  for (qint64 id : tabs) {
    if (auto* shell = tabToShell_.take(id)) {
      shell->releaseEmbedOwnershipForTab(id);
      shell->removeTab(id);
      destroyShellIfEmpty(shell);
    }
  }
  pendingFirstShell_.remove(session);
  sessions_.erase(std::remove_if(sessions_.begin(), sessions_.end(),
                                 [&](const std::unique_ptr<ClientSession>& p) {
                                   return p.get() == session;
                                 }),
                  sessions_.end());
}

void ShellApp::closeShell(ShellWindow* shell) {
  if (!shell) {
    return;
  }
  const auto tabs = shell->tabs();
  for (const auto& t : tabs) {
    if (t.isHome) {
      continue;
    }
    tabToShell_.remove(t.tabId);
    shell->releaseEmbedOwnershipForTab(t.tabId);
    shell->removeTab(t.tabId);
    if (t.session) {
      t.session->requestClose(t.tabId);
    }
  }

  // Drop from ownership list before force-close.
  for (auto it = shells_.begin(); it != shells_.end(); ++it) {
    if (it->get() == shell) {
      ShellWindow* raw = it->release();
      shells_.erase(it);
      raw->forceClose();
      raw->deleteLater();
      break;
    }
  }

  if (shells_.empty()) {
    QCoreApplication::quit();
  }
}

void ShellApp::closeTab(qint64 tabId) {
  if (tabId == kHomeTabId) {
    return;
  }
  auto* shell = tabToShell_.value(tabId, nullptr);
  if (!shell) {
    return;
  }
  ClientSession* session = nullptr;
  for (const auto& t : shell->tabs()) {
    if (t.tabId == tabId) {
      session = t.session;
      break;
    }
  }
  if (session) {
    session->requestClose(tabId);
  } else {
    // Orphan tab (no session): drop locally.
    tabToShell_.remove(tabId);
    shell->removeTab(tabId);
    destroyShellIfEmpty(shell);
  }
}

void ShellApp::activateTab(ShellWindow* shell, qint64 tabId) {
  shell->setActiveTab(tabId);
}

void ShellApp::tearOutTab(ShellWindow* source, qint64 tabId, QPoint globalPos) {
  if (!source || tabId == kHomeTabId) {
    return;
  }
  TabInfo moved;
  bool found = false;
  for (const auto& t : source->tabs()) {
    if (t.tabId == tabId) {
      moved = t;
      found = true;
      break;
    }
  }
  if (!found || moved.isHome) {
    return;
  }
  clearAllDropIndicators();
  source->releaseEmbedOwnershipForTab(tabId);
  source->removeTab(tabId);
  tabToShell_.remove(tabId);

  auto* neu = createShell(globalPos - QPoint(40, 20));
  tabToShell_.insert(tabId, neu);
  neu->addTab(moved);
  if (moved.session) {
    moved.session->notifyReattachment(neu->shellId());
  }
  neu->raise();
  neu->activateWindow();
  if (neu->embed()) {
    neu->embed()->resyncForeignWindow();
  }
  destroyShellIfEmpty(source);
}

void ShellApp::mergeTab(qint64 tabId, ShellWindow* target, int insertIndex) {
  if (tabId == kHomeTabId) {
    return;
  }
  auto* source = tabToShell_.value(tabId, nullptr);
  if (!source || source == target) {
    return;
  }
  TabInfo moved;
  for (const auto& t : source->tabs()) {
    if (t.tabId == tabId) {
      moved = t;
      break;
    }
  }
  if (!moved.tabId || moved.isHome) {
    return;
  }
  clearAllDropIndicators();
  source->releaseEmbedOwnershipForTab(tabId);
  source->removeTab(tabId);
  tabToShell_.insert(tabId, target);
  if (insertIndex < 0) {
    target->addTab(moved);
  } else {
    target->insertTab(moved, insertIndex);
  }
  if (moved.session) {
    moved.session->notifyReattachment(target->shellId());
  }
  target->raise();
  target->activateWindow();
  if (target->embed()) {
    target->embed()->resyncForeignWindow();
  }
  destroyShellIfEmpty(source);
}

ShellWindow* ShellApp::shellForTab(qint64 tabId) const {
  return tabToShell_.value(tabId, nullptr);
}

ShellWindow* ShellApp::shellAtGlobal(QPoint globalPos) const {
  for (const auto& shell : shells_) {
    if (shell && shell->isVisible() && shell->frameGeometry().contains(globalPos)) {
      return shell.get();
    }
  }
  return nullptr;
}

void ShellApp::destroyShellIfEmpty(ShellWindow* shell) {
  if (!shell || shell->clientTabCount() > 0) {
    return;
  }
  shell->setActiveTab(kHomeTabId);
  if (shells_.size() <= 1) {
    return;
  }
  for (auto it = shells_.begin(); it != shells_.end(); ++it) {
    if (it->get() == shell) {
      ShellWindow* raw = it->release();
      shells_.erase(it);
      raw->forceClose();
      raw->deleteLater();
      return;
    }
  }
}

QString ShellApp::makeTitle(int clientIndex, int windowIndex) const {
  return QStringLiteral("Client%1-Window%2").arg(clientIndex).arg(windowIndex);
}

}  // namespace mps::host
