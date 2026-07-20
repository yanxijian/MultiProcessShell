#pragma once

#include "envelope_channel.hpp"
#include "tab_info.hpp"

#include <QHash>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>

#include <memory>

namespace mps::host
{
	class ShellWindow;

	class ClientSession final : public QObject
	{
		Q_OBJECT
	public:
		ClientSession(int clientIndex, QString endpoint, QObject* parent = nullptr);
		~ClientSession() override;

		[[nodiscard]] int clientIndex() const
		{
			return clientIndex_;
		}
		[[nodiscard]] qint64 pageId() const
		{
			return pageId_;
		}
		[[nodiscard]] bool ready() const
		{
			return ready_;
		}
		[[nodiscard]] mps::ipc::EnvelopeChannel* channel()
		{
			return channel_.get();
		}

		void startClientProcess(const QString& clientExe, const QString& token);
		void attachSocket(QLocalSocket* socket);
		void requestCreateSubWindow(qint64 tabId, const QString& title);
		void requestActivate(qint64 tabId);
		void requestClose(qint64 tabId);
		void notifyReattachment(qint64 shellId);
		void setDragSuppress(bool on);
		[[nodiscard]] bool isDead() const
		{
			return dead_;
		}

	signals:
		void sessionHelloOk(ClientSession* self);
		void sessionReady(ClientSession* self);
		void subWindowAdded(ClientSession* self, qint64 tabId, QString title, quintptr wid);
		void subWindowRemoved(ClientSession* self, qint64 tabId);
		void sessionDead(ClientSession* self);
		void invokeNewWindow(ClientSession* self, qint64 sourceTabId);

	private:
		void onEnvelope(shell::ipc::v1::Envelope env);
		void sendHelloAck();
		void markDead();

		int clientIndex_ = 0;
		qint64 pageId_ = 0;
		QString endpoint_;
		bool ready_ = false;
		bool helloSeen_ = false;
		bool dead_ = false;
		QProcess* process_ = nullptr;
		QLocalSocket* socket_ = nullptr;
		std::unique_ptr<mps::ipc::EnvelopeChannel> channel_;
		// pending CreateSubWindow tab ids awaiting SubWindowAdded (same order)
		QList<qint64> pendingTabs_;
		QHash<qint64, quintptr> tabWids_;
		quintptr mainWid_ = 0;
	};
} // namespace mps::host
