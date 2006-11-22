#ifndef HOSTLABEL_H
#define HOSTLABEL_H

#include <QLabel>

class QMenu;

class HostLabel : public QLabel
{
	Q_OBJECT
	public:
		HostLabel(QWidget *parent=0);

	public slots:
		void setAddress(QString address);
		void disconnect();
		void copyAddress();

	protected:
		void contextMenuEvent(QContextMenuEvent *event);

	private:
		QString address_;
		QMenu *menu_;
};

#endif

