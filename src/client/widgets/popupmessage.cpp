/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2014 Calle Laakkonen

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

#include <QPainter>
#include <QBitmap>
#include <QApplication>
#include <QDesktopWidget>
#include <QTextDocument>
#include <QTextCursor>
#include <QTimer>

#include "popupmessage.h"

namespace widgets {

PopupMessage::PopupMessage(QWidget *parent)
	: QWidget(parent, Qt::ToolTip), _arrowoffset(0), _timer(new QTimer(this)), _doc(new QTextDocument(this))
{
	_timer->setSingleShot(true);
	_timer->setInterval(2500);

	connect(_timer, SIGNAL(timeout()), this, SLOT(hide()));
}

void PopupMessage::setMessage(const QString& message)
{
	if(!isVisible())
		_doc->clear();

	QTextCursor cursor(_doc);
	cursor.movePosition(QTextCursor::End);
	if(cursor.position()>0)
		cursor.insertBlock();

	cursor.insertHtml(message);

	// Force tooltip text color
	QTextCharFormat fmt;
	fmt.setForeground(palette().toolTipText());
	cursor.select(QTextCursor::BlockUnderCursor);
	cursor.mergeCharFormat(fmt);
}

namespace {
	static const int PADDING = 4;
	static const int ARROW_H = 10;
}

void PopupMessage::showMessage(const QPoint& point, const QString &message)
{
	setMessage(message);

	resize(_doc->size().toSize() + QSize(PADDING*2, PADDING*2 + ARROW_H));

	QRect rect(point - QPoint(width() - width()/6,height()), size());
	const QRect screen = qApp->desktop()->availableGeometry(parentWidget());

	// Make sure the popup fits horizontally
	if(rect.x() + rect.width() > screen.x() + screen.width()) {
		rect.moveRight(screen.x() + screen.width() - 1);
	} else if(rect.x() < screen.x()) {
		rect.moveLeft(screen.x());
	}
	_arrowoffset = point.x() - rect.x();

	// Make sure the popup fits vertically
	if(rect.y() + rect.height() > screen.y() + screen.height())
		rect.moveBottom(screen.y() + screen.height() - 1);
	else if(rect.y() < screen.y())
		rect.moveTop(screen.y());

	// Redraw the background and show on screen
	redrawBubble();
	move(rect.topLeft());
	show();
	_timer->start();
}

void PopupMessage::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setBrush(palette().toolTipBase());
	painter.drawPath(_bubble);

	painter.translate(PADDING, PADDING);
	_doc->drawContents(&painter);
}

void PopupMessage::redrawBubble()
{
	// Redraw the background bubble
	const qreal w = contentsRect().width()-1;
	const qreal h = contentsRect().height();
	const qreal rad = 4;
	const qreal h1 = h - ARROW_H;
	const qreal aw = 10;
	qreal arrowsafe = _arrowoffset;
	if(arrowsafe-aw < rad)
		arrowsafe = rad+aw;
	else if(arrowsafe+aw > w-rad)
		arrowsafe = w-rad-aw;

	_bubble = QPainterPath(QPointF(w-rad, 0));
	_bubble.cubicTo(w-rad/2, 0, w, rad/2, w, rad);
	_bubble.lineTo(w, h1-rad);
	_bubble.cubicTo(w, h1-rad/2, w-rad/2, h1, w-rad, h1);

	_bubble.lineTo(arrowsafe+aw, h1);
	_bubble.lineTo(arrowsafe, h);
	_bubble.lineTo(arrowsafe-aw, h1);

	_bubble.lineTo(rad, h1);
	_bubble.cubicTo(rad/2, h1, 0, h1-rad/2, 0, h1-rad);
	_bubble.lineTo(0, rad);
	_bubble.cubicTo(0, rad/2, rad/2, 0, rad, 0);
	_bubble.closeSubpath();

	// Set the widget transparency mask
	QBitmap mask(width(), height());
	mask.fill(Qt::color0);
	QPainter painter(&mask);
	painter.setBrush(Qt::color1);
	painter.drawPath(_bubble);
	setMask(mask);
}

}

