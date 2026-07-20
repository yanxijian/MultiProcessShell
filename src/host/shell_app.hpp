#pragma once

#include "client_session.hpp"
#include "shell_window.hpp"

#include <QLocalServer>
#include <QObject>
#include <QHash>
#include <memory>
#include <vector>

namespace mps::host {

class ShellApp final : public QObject {
  Q_OBJECT
public:
  explicit ShellApp(QString clientExe, QObject* parent = nullptr);
  [[nodiscard]] ShellWindow* createShell(QPoint pos = {});
  void createClientOn(ShellWindow* shell);
  void closeTab(qint64 tabId);
  void activateTab(ShellWindow* shell, qint64 tabId);
  void tearOutTab(ShellWindow* source, qint64 tabId, QPoint globalPos);
  void mergeTab(qint64 tabId, ShellWindow* target, int insertIndex = -1);
  void closeShell(ShellWindow* shell);
  void clearAllDropIndicators();
  [[nodiscard]] ShellWindow* shellForTab(qint64 tabId) const;
  [[nodiscard]] ShellWindow* shellAtGlobal(QPoint globalPos) const;
  [[nodiscard]] ShellWindow* shellFromChromeTarget(QObject* watched) const;
  void destroyShellIfEmpty(ShellWindow* shell);

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  void onNewConnection();
  void bindShell(ShellWindow* shell);
  void onSessionReady(ClientSession* session);
  void onSubWindowAdded(ClientSession* session, qint64 tabId, QString title, quintptr wid);
  void onSubWindowRemoved(ClientSession* session, qint64 tabId);
  void onSessionDead(ClientSession* session);
  [[nodiscard]] QString makeTitle(int clientIndex, int windowIndex) const;

  QString clientExe_;
  QString endpoint_;
  QString token_;
  QLocalServer* server_ = nullptr;
  std::vector<std::unique_ptr<ShellWindow>> shells_;
  std::vector<std::unique_ptr<ClientSession>> sessions_;
  QHash<qint64, ShellWindow*> tabToShell_;
  qint64 nextTabId_ = 1;
  int nextClientIndex_ = 1;
  QHash<int, int> nextWindowIndex_;  // clientIndex -> next M
  // Queued first CreateSubWindow per session after ready
  QHash<ClientSession*, ShellWindow*> pendingFirstShell_;
};

}  // namespace mps::host
