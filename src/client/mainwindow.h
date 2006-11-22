#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QActionGroup;
class NetStatus;
class HostLabel;

class MainWindow : public QMainWindow {
	Q_OBJECT
	public:
		MainWindow();

	protected:
		void closeEvent(QCloseEvent *event);

	private:
		void readSettings();
		void writeSettings();

		void initActions();
		void createMenus();
		void createToolbars();

		NetStatus *netstatus_;
		HostLabel *hostaddress_;

		QAction *save_;
		QAction *saveas_;
		QAction *quit_;

		QAction *host_;
		QAction *join_;
		QAction *logout_;

		QActionGroup *adminTools_;
		QAction *lockboard_;
		QAction *lockuser_;
		QAction *kickuser_;

		QActionGroup *drawingTools_;
		QAction *brushTool_;
		QAction *eraserTool_;
		QAction *zoomin_;
		QAction *zoomout_;

		QAction *toggleFileBar;
		QAction *toggleDrawBar;

		QAction *help_;
		QAction *about_;
};

#endif

