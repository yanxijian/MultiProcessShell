#include "shell_app.hpp"

#include <QLocalSocket>
#include <QUuid>
#include <QGuiApplication>
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

  shell->setAcceptDrops(true);
  shell->installEventFilter(this);
}

bool ShellApp::eventFilter(QObject* watched, QEvent* event) {
  auto* shell = qobject_cast<ShellWindow*>(watched);
  if (!shell) {
    return QObject::eventFilter(watched, event);
  }

  auto globalPosOf = [shell](QDropEvent* de) -> QPoint {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return shell->mapToGlobal(de->position().toPoint());
#else
    return shell->mapToGlobal(de->pos());
#endif
  };

  if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
    auto* de = static_cast<QDragMoveEvent*>(event);
    if (!de->mimeData()->hasFormat(QString::fromUtf8(kTabMime))) {
      return false;
    }
    // Only the title/tab chrome is a merge target. Client content must NOT accept
    // drops — otherwise release there becomes a no-op MoveAction and tear-out never runs.
    if (shell->isOverChrome(globalPosOf(de))) {
      de->acceptProposedAction();
    } else {
      de->setDropAction(Qt::IgnoreAction);
      de->ignore();
    }
    return true;
  }
  if (event->type() == QEvent::Drop) {
    auto* de = static_cast<QDropEvent*>(event);
    if (!de->mimeData()->hasFormat(QString::fromUtf8(kTabMime))) {
      return false;
    }
    if (!shell->isOverChrome(globalPosOf(de))) {
      de->setDropAction(Qt::IgnoreAction);
      de->ignore();
      return true;
    }
    const qint64 tabId = de->mimeData()->data(QString::fromUtf8(kTabMime)).toLongLong();
    auto* source = shellForTab(tabId);
    if (source == shell) {
      // Put back on the same chrome: stay; do not tear out.
      de->acceptProposedAction();
      return true;
    }
    mergeTab(tabId, shell);
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
  connect(raw, &ClientSession::invokeNewWindow, this, [this](ClientSession* session) {
    const int m = nextWindowIndex_[session->clientIndex()]++;
    const qint64 tabId = nextTabId_++;
    const QString title = makeTitle(session->clientIndex(), m);
    // Place on shell that already hosts this session if any.
    ShellWindow* shell = nullptr;
    for (auto& s : shells_) {
      for (const auto& t : s->tabs()) {
        if (t.session == session) {
          shell = s.get();
          break;
        }
      }
      if (shell) {
        break;
      }
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
    // Additional window: put on shell that currently has a tab of this session, else first shell.
    for (auto& s : shells_) {
      for (const auto& t : s->tabs()) {
        if (t.session == session) {
          shell = s.get();
          break;
        }
      }
      if (shell) {
        break;
      }
    }
  }
  if (!shell && !shells_.empty()) {
    shell = shells_.front().get();
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
  QList<qint64> tabs;
  for (auto it = tabToShell_.begin(); it != tabToShell_.end(); ++it) {
    // find tabs of this session
  }
  for (auto& shell : shells_) {
    const auto copy = shell->tabs();
    for (const auto& t : copy) {
      if (t.session == session) {
        tabs.push_back(t.tabId);
      }
    }
  }
  for (qint64 id : tabs) {
    if (auto* shell = tabToShell_.take(id)) {
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
  destroyShellIfEmpty(source);
}

void ShellApp::mergeTab(qint64 tabId, ShellWindow* target) {
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
  source->removeTab(tabId);
  tabToShell_.insert(tabId, target);
  target->addTab(moved);
  if (moved.session) {
    moved.session->notifyReattachment(target->shellId());
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
      raw->close();
      raw->deleteLater();
      return;
    }
  }
}

QString ShellApp::makeTitle(int clientIndex, int windowIndex) const {
  return QStringLiteral("Client%1-Window%2").arg(clientIndex).arg(windowIndex);
}

}  // namespace mps::host
