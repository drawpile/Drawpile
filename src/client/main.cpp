#include <QApplication>

#include "mainwindow.h"

int main(int argc, char *argv[]) {
	QApplication app(argc,argv);

	// These are used by QSettings
	app.setOrganizationName("drawpile");
	app.setOrganizationDomain("drawpile.sourceforge.net");
	app.setApplicationName("DrawPile");

	// Create and show the main window
	MainWindow win;
	win.show();

	return app.exec();
}

