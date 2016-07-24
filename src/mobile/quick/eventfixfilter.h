#ifndef EVENTFIXFILTER_H
#define EVENTFIXFILTER_H

#include <QObject>

class QWidget;

class TabletState;

class EventFixFilter : public QObject
{
	Q_OBJECT
public:
	explicit EventFixFilter(TabletState *ts, QWidget *container);

protected:
	bool eventFilter(QObject *watched, QEvent *event);

private:
	TabletState *m_tabletstate;
	QWidget *m_container;
};

#endif // EVENTFIXFILTER_H
