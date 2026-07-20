#pragma once

#include "envelope_channel.hpp"

#include <QHash>
#include <QLocalSocket>
#include <QObject>
#include <QWidget>

#include <memory>

namespace mps::client
{
	class PageWindow final : public QWidget
	{
		Q_OBJECT
	public:
		PageWindow(qint64 tabId, QString title, QWidget* parent = nullptr);
		[[nodiscard]] qint64 tabId() const
		{
			return tabId_;
		}

	signals:
		void requestNewWindow();

	private:
		qint64 tabId_ = 0;
	};

	class ClientApp final : public QObject
	{
		Q_OBJECT
	public:
		ClientApp(QString endpoint, QString token, QObject* parent = nullptr);
		[[nodiscard]] bool connectToHost();

	private:
		void onEnvelope(shell::ipc::v1::Envelope env);
		void sendHello();
		void ensureMainReported();
		void createPage(qint64 tabId, const QString& title);
		void closePage(qint64 tabId);
		void activatePage(qint64 tabId);

		QString endpoint_;
		QString token_;
		QLocalSocket* socket_ = nullptr;
		std::unique_ptr<mps::ipc::EnvelopeChannel> channel_;
		bool mainReported_ = false;
		QHash<qint64, PageWindow*> pages_;
		PageWindow* active_ = nullptr;
	};
} // namespace mps::client
