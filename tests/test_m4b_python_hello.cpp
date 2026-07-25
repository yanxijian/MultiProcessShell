#include "envelope_builder.hpp"
#include "envelope_channel.hpp"
#include "shell/ipc/v1/ipc.pb.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QUuid>

#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

namespace
{
	QString findPython()
	{
		const QByteArray fromEnv = qgetenv("PYTHON");
		if (!fromEnv.isEmpty())
		{
			return QString::fromLocal8Bit(fromEnv);
		}
#ifdef Q_OS_WIN
		return QStringLiteral("python");
#else
		return QStringLiteral("python3");
#endif
	}

	QString helloClientScript()
	{
		return QDir(QString::fromUtf8(MPS_SOURCE_DIR)).filePath(QStringLiteral("clients/python/hello_client.py"));
	}
} // namespace

TEST(M4bPythonHello, HelloAckOverLocalSocket)
{
	int argc = 1;
	char arg0[] = "mps_m4b_python_hello";
	char* argv[] = {arg0, nullptr};
	QCoreApplication app(argc, argv);

	const QString endpoint = QStringLiteral("mps-m4b-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
	QLocalServer::removeServer(endpoint);
	QLocalServer server;
	ASSERT_TRUE(server.listen(endpoint)) << qPrintable(server.errorString());

	QProcess py;
	py.setProgram(findPython());
	py.setArguments({helloClientScript(), QStringLiteral("--endpoint"), endpoint, QStringLiteral("--server-path"), server.fullServerName(),
					 QStringLiteral("--timeout"), QStringLiteral("15")});
	py.setProcessChannelMode(QProcess::MergedChannels);
	py.start();
	ASSERT_TRUE(py.waitForStarted(5000)) << "failed to start Python hello_client";

	ASSERT_TRUE(server.waitForNewConnection(10000)) << "Python did not connect";
	QLocalSocket* sock = server.nextPendingConnection();
	ASSERT_NE(sock, nullptr);

	bool gotHello = false;
	auto channel = std::make_unique<mps::ipc::EnvelopeChannel>(sock);
	channel->setHandler(
		[&](shell::ipc::v1::Envelope env)
		{
			if (!env.has_hello() || gotHello)
			{
				return;
			}
			gotHello = true;
			EXPECT_EQ(env.hello().caps().embed(), shell::ipc::v1::EMBED_NONE);
			auto ack =
				mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_EVT, QDateTime::currentMSecsSinceEpoch());
			auto* body = ack.mutable_hello_ack();
			body->set_protocol(1);
			body->set_session_id("m4b");
			body->mutable_host_caps()->set_embed(shell::ipc::v1::EMBED_HWND);
			ASSERT_TRUE(channel->send(ack));
		});

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
	while (py.state() == QProcess::Running && std::chrono::steady_clock::now() < deadline)
	{
		app.processEvents(QEventLoop::AllEvents, 50);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	if (py.state() == QProcess::Running)
	{
		py.kill();
		py.waitForFinished(2000);
		FAIL() << "Python hello_client timed out; output:\n" << py.readAll().constData();
	}

	EXPECT_TRUE(gotHello);
	EXPECT_EQ(py.exitCode(), 0) << py.readAll().constData();
}
