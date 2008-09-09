#include <QCoreApplication>

#include "../shared/server/server.h"
#include "../shared/net/constants.h"

using server::Server;

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);

	Server *server = new Server();
	server->start(protocol::DEFAULT_PORT);

	return app.exec();
}

