/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2013 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cmath>

#include <QApplication>
#include <QPainter>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>

#include "dualcolorbutton.h"

#ifndef DESIGNER_PLUGIN
namespace widgets {
#endif

DualColorButton::DualColorButton(QWidget *parent)
	: QWidget(parent), foreground_(Qt::black), background_(Qt::white), hilite_(0)
{
	setMinimumSize(32,32);
	setAcceptDrops(true);
	setMouseTracking(true);
}

DualColorButton::DualColorButton(const QColor& fgColor, const QColor& bgColor, QWidget *parent)
	: QWidget(parent), foreground_(fgColor), background_(bgColor)
{
	setMinimumSize(32,32);
	setAcceptDrops(true);
}

/**
 * The foregroundColorChanged signal is emitted
 * @param c color to set
 */
void DualColorButton::setForeground(const QColor &c)
{
	foreground_ = c;
	emit foregroundChanged(c);
	update();
}

/**
 * The backgroundColorChanged signal is emitted
 * @param c color to set
 */
void DualColorButton::setBackground(const QColor &c)
{
	background_ = c;
	emit backgroundChanged(c);
	update();
}

/**
 * Foreground and background colors switch places and signals are emitted.
 */
void DualColorButton::swapColors()
{
	const QColor tmp = foreground_;
	foreground_ = background_;
	background_ = tmp;
	emit foregroundChanged(foreground_);
	emit backgroundChanged(background_);
	update();
}

QRectF DualColorButton::foregroundRect() const
{
	// foreground rectangle fills the upper left two thirds of the widget
	return QRectF(
			0.5,
			0.5,
			ceil(width()/3.0*2),
			ceil(height()/3.0*2)
	);
}

QRectF DualColorButton::backgroundRect() const
{
	// Background rectangle fills the lower right two thirds of the widget.
	// It is partially obscured by the foreground rectangle
	const float x = width() / 3.0;
	const float y = height() / 3.0;
	return QRectF(
		floor(x-1) + 0.5,
		floor(y-1) + 0.5,
		ceil(x*2),
		ceil(y*2)
	);
}

QRectF DualColorButton::resetBlackRect() const
{
	const float w = floor(width() / (3.0 * (3.0 / 2.0)) - 2);
	const float h = floor(height() / (3.0 * (3.0 / 2.0)) - 2);
	const float x = 1;
	const float y = ceil(1 + height() * (2.0 / 3.0));
	return QRectF(x+0.5, y+0.5, w, h);
}

QRectF DualColorButton::resetWhiteRect() const
{
	QRectF r = resetBlackRect();
	r.translate(floor(r.width()/2.0), ceil(r.height()/2.0));
	return r;
}

void DualColorButton::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);

	QPen normal = palette().color(QPalette::Mid);
	QPen hilite = palette().color(QPalette::Light);

	painter.setRenderHint(QPainter::Antialiasing);

	// Draw background box
	const QRectF bgbox = backgroundRect();
	painter.setPen(hilite_==2 ? hilite : normal);
	painter.setBrush(background_);
	painter.drawRoundedRect(bgbox, 3, 3);

	// Draw foreground box
	const QRectF fgbox = foregroundRect();
	painter.setPen(hilite_==1 ? hilite : normal);
	painter.setBrush(foreground_);
	painter.drawRoundedRect(fgbox, 3, 3);

	// Draw reset boxes
	painter.setPen(hilite_==4 ? hilite : normal);
	painter.setBrush(Qt::white);
	painter.drawRect(resetWhiteRect());
	painter.setBrush(Qt::black);
	painter.drawRect(resetBlackRect());

	// Draw swap arrow
	static const QPointF arrows[] = {
		QPointF(0, 2),
		QPointF(2, 4),
		QPointF(2, 3),
		QPointF(4, 3),
		QPointF(4, 5),
		QPointF(3, 5),
		QPointF(5, 7),
		QPointF(7, 5),
		QPointF(6, 5),
		QPointF(6, 1),
		QPointF(2, 1),
		QPointF(2, 0)
	};
	const qreal scale = (1.0 / 8.0) * width() / 3.0;
	painter.translate(ceil(fgbox.width()) + 0.0, 0);
	painter.scale(scale, scale);
	painter.setPen(hilite_==3 ? hilite : normal);
	painter.setBrush(palette().dark());
	painter.drawConvexPolygon(arrows, 12);

}

void DualColorButton::mousePressEvent(QMouseEvent *event)
{
	if(event->button() != Qt::LeftButton)
		return;
	dragSource_ = NODRAG;
	if(backgroundRect().contains(event->pos()))
			dragSource_ = BACKGROUND;
	if(foregroundRect().contains(event->pos()))
			dragSource_ = FOREGROUND;

	dragStart_ = event->pos();
}

void DualColorButton::mouseMoveEvent(QMouseEvent *event)
{
	if(dragSource_ != NODRAG && (event->buttons() & Qt::LeftButton) &&
			(event->pos() - dragStart_).manhattanLength()
		> QApplication::startDragDistance())
	{
		QDrag *drag = new QDrag(this);

		QMimeData *mimedata = new QMimeData;
		const QColor color = (dragSource_ == FOREGROUND)?foreground_:background_;
		mimedata->setColorData(color);

		drag->setMimeData(mimedata);
		drag->start(Qt::CopyAction);
	}
	// Higlight clickable areas
	QRectF fgr = foregroundRect();
	QRectF bgr = backgroundRect();
	if(fgr.contains(event->pos()))
		hilite_ = 1;
	else if(bgr.contains(event->pos()))
		hilite_ = 2;
	else if(event->pos().x() > fgr.right() && event->pos().y() < bgr.top())
		hilite_ = 3;
	else if(event->pos().x() < bgr.left() && event->pos().y() > fgr.bottom())
		hilite_ = 4;
	else
		hilite_ = 0;
	update();
}

void DualColorButton::leaveEvent(QEvent *event)
{
	QWidget::leaveEvent(event);
	hilite_ = 0;
	update();
}

void DualColorButton::mouseReleaseEvent(QMouseEvent *event)
{
	if(event->button() != Qt::LeftButton)
		return;
	QRectF fgr = foregroundRect();
	QRectF bgr = backgroundRect();
	if(fgr.contains(event->pos()))
		emit foregroundClicked(foreground_);
	else if(bgr.contains(event->pos()))
		emit backgroundClicked(background_);
	else if(event->pos().x() > fgr.right() && event->pos().y() < bgr.top())
		swapColors();
	else if(event->pos().x() < bgr.left() && event->pos().y() > fgr.bottom()) {
		foreground_ = Qt::black;
		background_ = Qt::white;
		emit foregroundChanged(foreground_);
		emit backgroundChanged(background_);
		update();
	}
}

void DualColorButton::dragEnterEvent(QDragEnterEvent *event)
{
	if(event->mimeData()->hasFormat("application/x-color"))
		event->acceptProposedAction();
}

void DualColorButton::dropEvent(QDropEvent *event)
{
	const QColor color = qvariant_cast<QColor>(event->mimeData()->colorData());
	if(foregroundRect().contains(event->pos())) {
		foreground_ = color;
		emit foregroundChanged(color);
	} else if(backgroundRect().contains(event->pos())) {
		background_ = color;
		emit backgroundChanged(color);
	}
	update();
}

#ifndef DESIGNER_PLUGIN
}
#endif

