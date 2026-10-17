#include "client_session.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <QUuid>

#include <gtest/gtest.h>

namespace
{
	QCoreApplication* ensureCoreApp()
	{
		if (QCoreApplication::instance())
		{
			return qobject_cast<QCoreApplication*>(QCoreApplication::instance());
		}
		static int argc = 1;
		static char name[] = "mps_session_tests";
		static char* argv[] = {name, nullptr};
		static QCoreApplication app(argc, argv);
		return &app;
	}
} // namespace

TEST(ClientSessionInvoke, TimesOutExactlyOnce)
{
	QCoreApplication* app = ensureCoreApp();
	ASSERT_NE(app, nullptr);

	const QString endpoint = QStringLiteral("mps-invoke-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
	QLocalServer::removeServer(endpoint);
	QLocalServer server;
	ASSERT_TRUE(server.listen(endpoint)) << qPrintable(server.errorString());

	QLocalSocket client;
	client.connectToServer(endpoint);
	ASSERT_TRUE(client.waitForConnected(2000)) << qPrintable(client.errorString());
	ASSERT_TRUE(server.waitForNewConnection(2000));
	QLocalSocket* peer = server.nextPendingConnection();
	ASSERT_NE(peer, nullptr);

	mps::host::ClientSession session(1, endpoint);
	session.setInvokeTimeoutMs(120);
	session.attachSocket(peer);

	int calls = 0;
	bool lastOk = true;
	QString lastError;
	session.sendInvoke(QStringLiteral("test.never_replies"), {}, 0,
					   [&](bool ok, const QByteArray&, const QString& error)
					   {
						   ++calls;
						   lastOk = ok;
						   lastError = error;
					   });

	QEventLoop loop;
	QTimer::singleShot(600, &loop, &QEventLoop::quit);
	loop.exec();

	EXPECT_EQ(calls, 1);
	EXPECT_FALSE(lastOk);
	EXPECT_TRUE(lastError.contains(QStringLiteral("timed out"))) << qPrintable(lastError);

	// The timeout entry is erased before the callback; nothing may fire twice.
	QEventLoop settle;
	QTimer::singleShot(250, &settle, &QEventLoop::quit);
	settle.exec();
	EXPECT_EQ(calls, 1);
}
