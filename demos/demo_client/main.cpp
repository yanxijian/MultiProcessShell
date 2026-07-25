#include "client_app.hpp"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	QCommandLineParser parser;
	parser.addHelpOption();
	QCommandLineOption fromHost(QStringLiteral("from-host"));
	QCommandLineOption endpoint(QStringLiteral("endpoint"), QString(), QStringLiteral("name"));
	QCommandLineOption token(QStringLiteral("pipe-token"), QString(), QStringLiteral("token"));
	QCommandLineOption protocol(QStringLiteral("protocol"), QString(), QStringLiteral("n"), QStringLiteral("1"));
	QCommandLineOption noHeartbeat(QStringLiteral("no-heartbeat"), QStringLiteral("Disable Client Heartbeat (M6 repro)"));
	parser.addOption(fromHost);
	parser.addOption(endpoint);
	parser.addOption(token);
	parser.addOption(protocol);
	parser.addOption(noHeartbeat);
	parser.process(app);

	if (!parser.isSet(fromHost) || !parser.isSet(endpoint))
	{
		qWarning("demo_client must be started by the Host (--from-host --endpoint=...)");
		return 2;
	}

	mps::client::ClientApp client(parser.value(endpoint), parser.value(token), !parser.isSet(noHeartbeat));
	if (!client.connectToHost())
	{
		return 3;
	}
	return app.exec();
}
