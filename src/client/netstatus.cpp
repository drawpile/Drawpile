#include <QPaintEvent>
#include <QPainter>

#include "netstatus.h"

NetStatus::NetStatus(QWidget *parent)
	: QWidget(parent), icon_(":/icons/network-error.png")
{
	setMinimumSize(icon_.size());
}

void NetStatus::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	painter.drawPixmap(event->rect(), icon_, event->rect());
}

