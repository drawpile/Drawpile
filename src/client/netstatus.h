#ifndef NETSTATUS_H
#define NETSTATUS_H

#include <QWidget>
#include <QPixmap>

class NetStatus : public QWidget
{
	Q_OBJECT
	public:
		NetStatus(QWidget *parent);
	protected:
		void paintEvent(QPaintEvent *event);
	private:
		QPixmap icon_;
};

#endif

