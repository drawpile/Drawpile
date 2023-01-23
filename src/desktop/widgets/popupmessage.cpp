/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2017 Calle Laakkonen

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
#include <QScreen>
#include <QTextDocument>
#include <QTextCursor>
#include <QTimer>

#include "desktop/widgets/popupmessage.h"

namespace widgets {

PopupMessage::PopupMessage(QWidget *parent)
	: QWidget(nullptr, Qt::ToolTip), m_arrowoffset(0), m_timer(new QTimer(this)), m_doc(new QTextDocument(this)), m_parentWidget(parent)
{
	m_timer->setSingleShot(true);
	m_timer->setInterval(2500);

	connect(m_timer, &QTimer::timeout, this, &PopupMessage::hide);

	// If this widget has a real parent, showing it will raise the parent window
	// (on Windows,) which is undesirable.
	connect(m_parentWidget, &QWidget::destroyed, this, &PopupMessage::deleteLater);
}

void PopupMessage::setMessage(const QString &message)
{
	static const int MAX_LINES = 6;
	if(!isVisible())
		m_doc->clear();

	QTextCursor cursor(m_doc);
	cursor.movePosition(QTextCursor::End);
	if(cursor.position()>0)
		cursor.insertBlock();

	cursor.insertHtml(message);

	// Force tooltip text color
	QTextCharFormat fmt;
	fmt.setForeground(palette().toolTipText());
	cursor.select(QTextCursor::BlockUnderCursor);
	cursor.mergeCharFormat(fmt);

	// Limit the number of lines
	while(m_doc->lineCount() > MAX_LINES) {
		cursor.movePosition(QTextCursor::Start);
		cursor.select(QTextCursor::LineUnderCursor);
		cursor.removeSelectedText();
		cursor.deleteChar(); // remove the newline too
	}

}

namespace {
	static const int PADDING = 4;
	static const int ARROW_H = 10;
}

void PopupMessage::showMessage(const QPoint& point, const QString &message)
{
	setMessage(message);

	resize(m_doc->size().toSize() + QSize(PADDING*2, PADDING*2 + ARROW_H));

	QRect rect(point - QPoint(width() - width()/6,height()), size());

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	const QRect screen = m_parentWidget->screen()->availableGeometry();
#else
	const QRect screen = qApp->primaryScreen()->availableGeometry();
#endif

	// Make sure the popup fits horizontally
	if(rect.x() + rect.width() > screen.x() + screen.width()) {
		rect.moveRight(screen.x() + screen.width() - 1);
	} else if(rect.x() < screen.x()) {
		rect.moveLeft(screen.x());
	}
	m_arrowoffset = point.x() - rect.x();

	// Make sure the popup fits vertically
	if(rect.y() + rect.height() > screen.y() + screen.height())
		rect.moveBottom(screen.y() + screen.height() - 1);
	else if(rect.y() < screen.y())
		rect.moveTop(screen.y());

	// Redraw the background and show on screen
	redrawBubble();
	move(rect.topLeft());
	show();
	m_timer->start();
}

void PopupMessage::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setBrush(palette().toolTipBase());
	painter.drawPath(m_bubble);

	painter.translate(PADDING, PADDING);
	m_doc->drawContents(&painter);
}

void PopupMessage::redrawBubble()
{
	// Redraw the background bubble
	const qreal w = contentsRect().width()-1;
	const qreal h = contentsRect().height();
	const qreal rad = 4;
	const qreal h1 = h - ARROW_H;
	const qreal aw = 10;
	qreal arrowsafe = m_arrowoffset;
	if(arrowsafe-aw < rad)
		arrowsafe = rad+aw;
	else if(arrowsafe+aw > w-rad)
		arrowsafe = w-rad-aw;

	m_bubble = QPainterPath(QPointF(w-rad, 0));
	m_bubble.cubicTo(w-rad/2, 0, w, rad/2, w, rad);
	m_bubble.lineTo(w, h1-rad);
	m_bubble.cubicTo(w, h1-rad/2, w-rad/2, h1, w-rad, h1);

	m_bubble.lineTo(arrowsafe+aw, h1);
	m_bubble.lineTo(arrowsafe, h);
	m_bubble.lineTo(arrowsafe-aw, h1);

	m_bubble.lineTo(rad, h1);
	m_bubble.cubicTo(rad/2, h1, 0, h1-rad/2, 0, h1-rad);
	m_bubble.lineTo(0, rad);
	m_bubble.cubicTo(0, rad/2, rad/2, 0, rad, 0);
	m_bubble.closeSubpath();

	// Set the widget transparency mask
	QBitmap mask(width(), height());
	mask.fill(Qt::color0);
	QPainter painter(&mask);
	painter.setBrush(Qt::color1);
	painter.drawPath(m_bubble);
	setMask(mask);
}

}

