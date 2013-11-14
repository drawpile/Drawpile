
/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2008 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QPainter>
#include <QLabel>
#include <QBitmap>
#include <QApplication>
#include <QDesktopWidget>

#include "popupmessage.h"

namespace widgets {

PopupMessage::PopupMessage(QWidget *parent)
	: QWidget(parent, Qt::ToolTip), arrowoffset_(0)
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	message_ = new QLabel(this);
	message_->setAlignment(Qt::AlignCenter);
	message_->setTextFormat(Qt::RichText);
	layout->addWidget(message_);
#if (QT_VERSION >= QT_VERSION_CHECK(4, 4,0)) // Not supported by QT<4.4?
	// We should really do the drawing ourselves for best results.
	layout->addSpacerItem(new QSpacerItem(30, 30, QSizePolicy::MinimumExpanding));
#else
	layout->addSpacing(30);
#endif
	resize(200,60);
	timer_.setSingleShot(true);
	timer_.setInterval(2500);
	connect(&timer_, SIGNAL(timeout()), this, SLOT(hide()));
}

/**
 * @param message message to display
 */
void PopupMessage::setMessage(const QString& message)
{
	if(isVisible())
		message_->setText(message_->text() + "<br>" + message);
	else
		message_->setText(message);
}

/**
 * The message will popup at a position where it fits completely on the screen.
 * The position of the arrow is adjusted so it points at the given point.
 * @param point popup coordinates
 */
void PopupMessage::popupAt(const QPoint& point)
{
	QRect rect(point - QPoint(width() - width()/6,height()), size());
	const QRect screen = qApp->desktop()->availableGeometry(parentWidget());

	// Make sure the popup fits horizontally
	if(rect.x() + rect.width() > screen.x() + screen.width()) {
		rect.moveRight(screen.x() + screen.width() - 1);
	} else if(rect.x() < screen.x()) {
		rect.moveLeft(screen.x());
	}
	arrowoffset_ = point.x() - rect.x();

	// Make sure the popup fits vertically
	if(rect.y() + rect.height() > screen.y() + screen.height())
		rect.moveBottom(screen.y() + screen.height() - 1);
	else if(rect.y() < screen.y())
		rect.moveTop(screen.y());

	// Redraw the background and show on screen
	redrawBubble();
	move(rect.topLeft());
	show();
	timer_.start();
}

void PopupMessage::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setBrush(palette().light());
	painter.drawPath(bubble);
}

void PopupMessage::redrawBubble()
{
	// Redraw the background bubble
	const qreal w = contentsRect().width();
	const qreal h = contentsRect().height();
	const qreal x = w/6.0;
	const qreal h1 = h - h/4.0;
	const qreal aw = (h-h1) / 2.0;
	qreal arrowsafe = arrowoffset_;
	if(arrowsafe < x)
		arrowsafe = x;
	else if(arrowsafe+aw > w-x)
		arrowsafe = w-x-aw;

	bubble = QPainterPath(QPointF(x, 0));
	bubble.cubicTo(QPointF(0, 0), QPointF(0, h1), QPointF(x, h1));
	bubble.lineTo(QPointF(arrowsafe, h1));
	bubble.lineTo(QPointF(arrowoffset_, h));
	bubble.lineTo(QPointF(arrowsafe+aw, h1));
	bubble.lineTo(QPointF(w-x, h1));
	bubble.cubicTo(QPointF(w, h1), QPointF(w, 0), QPointF(w-x, 0));
	bubble.closeSubpath();

	// Set the widget transparency mask
	QBitmap mask(width(), height());
	mask.fill(Qt::color0);
	QPainter painter(&mask);
	painter.setBrush(Qt::color1);
	painter.drawPath(bubble);
	setMask(mask);
}

}

