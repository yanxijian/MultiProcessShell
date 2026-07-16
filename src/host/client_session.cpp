#include "client_session.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QLocalSocket>

namespace mps::host {
namespace {
qint64 g_nextPageId = 1;
}

ClientSession::ClientSession(int clientIndex, QString endpoint, QObject* parent)
    : QObject(parent),
      clientIndex_(clientIndex),
      pageId_(g_nextPageId++),
      endpoint_(std::move(endpoint)) {}

ClientSession::~ClientSession() {
  if (process_) {
    process_->disconnect(this);
    process_->kill();
    process_->waitForFinished(1000);
  }
  if (socket_) {
    socket_->disconnect(this);
    socket_->abort();
  }
}

void ClientSession::startClientProcess(const QString& clientExe, const QString& token) {
  process_ = new QProcess(this);
  connect(process_, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
    emit sessionDead(this);
  });
  QStringList args;
  args << QStringLiteral("--from-host")
       << QStringLiteral("--endpoint=%1").arg(endpoint_)
       << QStringLiteral("--pipe-token=%1").arg(token)
       << QStringLiteral("--protocol=1");
  process_->start(clientExe, args);
}

void ClientSession::attachSocket(QLocalSocket* socket) {
  socket_ = socket;
  socket_->setParent(this);
  channel_ = std::make_unique<mps::ipc::EnvelopeChannel>(socket_, this);
  channel_->setHandler([this](shell::ipc::v1::Envelope env) { onEnvelope(std::move(env)); });
  connect(channel_.get(), &mps::ipc::EnvelopeChannel::disconnected, this, [this] {
    emit sessionDead(this);
  });
}

void ClientSession::sendHelloAck() {
  shell::ipc::v1::Envelope env;
  env.set_protocol(1);
  env.set_id(mps::ipc::newCorrelationId());
  env.set_dir(shell::ipc::v1::DIR_EVT);
  env.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
  auto* ack = env.mutable_hello_ack();
  ack->set_protocol(1);
  ack->set_session_id(QString::number(pageId_).toStdString());
  auto* caps = ack->mutable_host_caps();
  caps->set_embed(shell::ipc::v1::EMBED_HWND);
  caps->set_tab_drag(true);
  caps->set_heartbeat(true);
  caps->set_invoke(true);
  caps->set_multi_sub_window(true);
  channel_->send(env);
}

void ClientSession::requestCreateSubWindow(qint64 tabId, const QString& title) {
  pendingTabs_.push_back(tabId);
  shell::ipc::v1::Envelope env;
  env.set_protocol(1);
  env.set_id(mps::ipc::newCorrelationId());
  env.set_dir(shell::ipc::v1::DIR_REQ);
  env.set_page_id(pageId_);
  env.set_tab_id(tabId);
  env.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
  env.mutable_create_sub_window()->set_title(title.toStdString());
  channel_->send(env);
}

void ClientSession::requestActivate(qint64 tabId) {
  shell::ipc::v1::Envelope env;
  env.set_protocol(1);
  env.set_id(mps::ipc::newCorrelationId());
  env.set_dir(shell::ipc::v1::DIR_EVT);
  env.set_page_id(pageId_);
  env.set_tab_id(tabId);
  env.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
  env.mutable_active_sub_window();
  channel_->send(env);
}

void ClientSession::requestClose(qint64 tabId) {
  shell::ipc::v1::Envelope env;
  env.set_protocol(1);
  env.set_id(mps::ipc::newCorrelationId());
  env.set_dir(shell::ipc::v1::DIR_REQ);
  env.set_page_id(pageId_);
  env.set_tab_id(tabId);
  env.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
  env.mutable_query_close_sub_window();
  channel_->send(env);
}

void ClientSession::notifyReattachment(qint64 shellId) {
  shell::ipc::v1::Envelope env;
  env.set_protocol(1);
  env.set_id(mps::ipc::newCorrelationId());
  env.set_dir(shell::ipc::v1::DIR_EVT);
  env.set_page_id(pageId_);
  env.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
  env.mutable_notify_main_window_reattachment()->set_shell_id(shellId);
  channel_->send(env);
}

void ClientSession::setDragSuppress(bool on) {
  shell::ipc::v1::Envelope env;
  env.set_protocol(1);
  env.set_id(mps::ipc::newCorrelationId());
  env.set_dir(shell::ipc::v1::DIR_EVT);
  env.set_page_id(pageId_);
  env.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
  env.mutable_set_drag_suppress()->set_suppress(on);
  channel_->send(env);
}

void ClientSession::onEnvelope(shell::ipc::v1::Envelope env) {
  if (env.has_hello() && !helloSeen_) {
    helloSeen_ = true;
    sendHelloAck();
    emit sessionHelloOk(this);
    return;
  }
  if (env.has_main_window_added()) {
    mainWid_ = static_cast<quintptr>(env.main_window_added().wid());
    ready_ = true;
    emit sessionReady(this);
    return;
  }
  if (env.has_sub_window_added()) {
    qint64 tabId = env.tab_id();
    if (tabId == 0 && !pendingTabs_.isEmpty()) {
      tabId = pendingTabs_.takeFirst();
    } else if (!pendingTabs_.isEmpty() && pendingTabs_.front() == tabId) {
      pendingTabs_.pop_front();
    }
    quintptr wid = static_cast<quintptr>(env.sub_window_added().wid());
    if (wid == 0) {
      wid = mainWid_;
    }
    tabWids_.insert(tabId, wid);
    const QString title = QString::fromStdString(env.sub_window_added().title());
    emit subWindowAdded(this, tabId, title, wid);
    return;
  }
  if (env.has_sub_window_removed()) {
    const qint64 tabId = env.tab_id();
    tabWids_.remove(tabId);
    emit subWindowRemoved(this, tabId);
    return;
  }
  if (env.has_query_close_sub_window_result()) {
    // Accept → tear down Host tab immediately; SubWindowRemoved is idempotent backup.
    if (env.query_close_sub_window_result().accept()) {
      const qint64 tabId = env.tab_id();
      tabWids_.remove(tabId);
      emit subWindowRemoved(this, tabId);
    }
    return;
  }
  if (env.has_invoke()) {
    // Client asks Host to create another window in this session.
    if (env.invoke().method() == "demo.request_new_window") {
      // Handled at ShellApp level via signal — forward through channel owner.
      emit invokeNewWindow(this);
      shell::ipc::v1::Envelope res;
      res.set_protocol(1);
      res.set_id(env.id());
      res.set_dir(shell::ipc::v1::DIR_RES);
      res.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
      res.mutable_invoke_result()->set_payload("ok");
      channel_->send(res);
      return;
    }
    shell::ipc::v1::Envelope res;
    res.set_protocol(1);
    res.set_id(env.id());
    res.set_dir(shell::ipc::v1::DIR_RES);
    res.set_ts_ms(QDateTime::currentMSecsSinceEpoch());
    auto* err = res.mutable_error();
    err->set_code(shell::ipc::v1::ERROR_UNIMPLEMENTED);
    err->set_message("Invoke not implemented in Demo Host");
    channel_->send(res);
    return;
  }
  if (env.has_heartbeat()) {
    return;
  }
}

}  // namespace mps::host
