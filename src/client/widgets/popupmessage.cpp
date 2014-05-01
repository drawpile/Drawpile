
/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2008 Calle Laakkonen

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
	layout->addSpacerItem(new QSpacerItem(30, 30, QSizePolicy::MinimumExpanding));
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
	painter.setBrush(palette().toolTipBase());
	painter.drawPath(bubble);
}

void PopupMessage::redrawBubble()
{
	// Redraw the background bubble
	const qreal w = contentsRect().width()-1;
	const qreal h = contentsRect().height();
	const qreal rad = 4;
	const qreal h1 = h - 10;
	const qreal aw = 10;
	qreal arrowsafe = arrowoffset_;
	if(arrowsafe-aw < rad)
		arrowsafe = rad+aw;
	else if(arrowsafe+aw > w-rad)
		arrowsafe = w-rad-aw;

	bubble = QPainterPath(QPointF(w-rad, 0));
	bubble.cubicTo(w-rad/2, 0, w, rad/2, w, rad);
	bubble.lineTo(w, h1-rad);
	bubble.cubicTo(w, h1-rad/2, w-rad/2, h1, w-rad, h1);

	bubble.lineTo(arrowsafe+aw, h1);
	bubble.lineTo(arrowsafe, h);
	bubble.lineTo(arrowsafe-aw, h1);

	bubble.lineTo(rad, h1);
	bubble.cubicTo(rad/2, h1, 0, h1-rad/2, 0, h1-rad);
	bubble.lineTo(0, rad);
	bubble.cubicTo(0, rad/2, rad/2, 0, rad, 0);
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

