#include <QPainter>
#include <QMouseEvent>

#include "dualcolorbutton.h"

namespace widgets {

DualColorButton::DualColorButton(QWidget *parent)
	: QWidget(parent), foreground_(Qt::black), background_(Qt::white)
{
	setMinimumSize(32,32);
}

DualColorButton::DualColorButton(const QColor& fgColor, const QColor& bgColor, QWidget *parent)
	: QWidget(parent), foreground_(fgColor), background_(bgColor)
{
	setMinimumSize(32,32);
}

/**
 * The foregroundColorChanged signal is emitted
 * @param c color to set
 */
void DualColorButton::setForeground(const QColor &c)
{
	foreground_ = c;
	update();
}

/**
 * The backgroundColorChanged signal is emitted
 * @param c color to set
 */
void DualColorButton::setBackground(const QColor &c)
{
	background_ = c;
	update();
}

QRect DualColorButton::foregroundRect() const
{
	// foreground rectangle fills the upper left two thirds of the widget
	return QRect(0,0, qRound(width()/3.0*2), qRound(height()/3.0*2));
}

QRect DualColorButton::backgroundRect() const
{
	// Background rectangle filles the lower right two thirds of the widget.
	// It is partially obscured by the foreground rectangle
	int x = qRound(width() / 3.0);
	int y = qRound(height() / 3.0);
	return QRect(x-1,y-1, x*2-1, y*2-1);
}

QRect DualColorButton::resetBlackRect() const
{
	int x = qRound(width()/9.0);
	int y = qRound(height()/9.0*7);
	int w = qRound(width() / 9.0);
	int h = qRound(height() / 9.0);
	return QRect(x-w/3,y-h/3,w,h);
}

QRect DualColorButton::resetWhiteRect() const
{
	QRect r = resetBlackRect();
	r.translate(r.width()/2, r.height()/2);
	return r;
}

void DualColorButton::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);

	// Draw background box
	QRect bgbox = backgroundRect();
	painter.fillRect(bgbox, background_);
	painter.drawRect(bgbox);

	// Draw foreground box
	QRect fgbox = foregroundRect();
	painter.fillRect(fgbox, foreground_);
	painter.drawRect(fgbox);

	// Draw reset boxes
	QRect rwhite = resetWhiteRect();
	painter.fillRect(rwhite, Qt::white);
	painter.drawRect(rwhite);
	painter.fillRect(resetBlackRect(), Qt::black);

	// Draw swap arrow
	static const QPoint arrows[12] = {
		QPoint(3,1),
		QPoint(1,3),
		QPoint(3,5),
		QPoint(3,4),
		QPoint(6,4),
		QPoint(6,7),
		QPoint(5,7),
		QPoint(7,9),
		QPoint(9,7),
		QPoint(8,7),
		QPoint(8,2),
		QPoint(3,2)
	};

	painter.scale(width()/10.0/3.0, height()/10.0/3.0);
	painter.translate(20,0);
	painter.setBrush(palette().light());
	painter.drawConvexPolygon(arrows,12);

}

void DualColorButton::mousePressEvent(QMouseEvent *event)
{
	QRect swaprect(qRound(width()*2.0/3.0),0,width()/3,height()/3);
	if(resetBlackRect().contains(event->pos()) || resetWhiteRect().contains(event->pos())) {
		foreground_ = Qt::black;
		background_ = Qt::white;
		emit foregroundChanged(foreground_);
		emit backgroundChanged(background_);
		update();
	} else if(foregroundRect().contains(event->pos())) {
		emit foregroundClicked();
	} else if(backgroundRect().contains(event->pos())) {
		emit backgroundClicked();
	} else if(swaprect.contains(event->pos())) {
		QColor tmp = foreground_;
		foreground_ = background_;
		background_ = tmp;
		emit foregroundChanged(foreground_);
		emit backgroundChanged(background_);
		update();
	}
}

}

