#include <QCoreApplication>
#include <QStringList>
#include <iostream>

#include "config.h"

#include "../shared/server/server.h"

using std::cerr;
using server::Server;

void printHelp() {
	std::cout << "DrawPile standalone server. Usage:\n\n"
		"drawpile-srv [options]\n\n"
		"Options:\n"
		"\t--port, -p <port>           Listening port (default: "
		<< DRAWPILE_PROTO_DEFAULT_PORT << ")\n"
		"\t--listen, -l <address>      Listening address (default: all)\n"
		"\t--verbose, -v               Verbose mode\n";
}

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);

	int port = DRAWPILE_PROTO_DEFAULT_PORT;
	QHostAddress address = QHostAddress::Any;
	bool verbose = false;

	// Parse command line arguments
	// TODO
	QStringList args = app.arguments();
	for(int i=1;i<args.size();++i) {
		if(args[i] == "-h" || args[i] == "--help") {
			printHelp();
			return 0;
		} else if(args[i]=="--port" || args[i]=="-p") {
			if(i+1>=args.size()) {
				cerr << "Port number not specified\n";
				return 1;
			}
			bool ok;
			port = args[++i].toInt(&ok);
			if(!ok) {
				cerr << args[i].toUtf8().constData() << " is not a number.";
				return 1;
			}
		} else if(args[i]=="--listen" || args[i]=="-l") {
			if(i+1>=args.size()) {
				cerr << "Listening address not specified\n";
				return 1;
			}
			if(address.setAddress(args[++i])==false) {
				cerr << "Not a valid address: " << args[i].toUtf8().constData() << "\n";
				return 1;
			}
		} else if(args[i]=="--verbose" || args[i]=="-v") {
			verbose = true;
		} else {
			cerr << "Unrecognized argument: " << args[i].toUtf8().constData() << "\n";
			return 1;
		}
	}

	// Start the server
	Server *server = new Server();

	server->connect(server, SIGNAL(serverStopped()), &app, SLOT(quit()));

	server->setErrorStream(new QTextStream(stderr));
	if(verbose)
		server->setDebugStream(new QTextStream(stdout));

	if(!server->start(port, false, address))
		return 1;

	return app.exec();
}

