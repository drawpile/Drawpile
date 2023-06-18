// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QBitmap>
#include <QPainter>
#include <QScreen>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>

#include "desktop/utils/qtguicompat.h"
#include "desktop/widgets/popupmessage.h"

namespace widgets {

PopupMessage::PopupMessage(QWidget *parent)
	: QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint)
	, m_arrowoffset(0)
	, m_timer(new QTimer(this))
	, m_doc(new QTextDocument(this))
	, m_parentWidget(parent)
{
	setAttribute(Qt::WA_TranslucentBackground);
	m_timer->setSingleShot(true);
	m_timer->setInterval(2500);

	connect(m_timer, &QTimer::timeout, this, &PopupMessage::hide);

	// If this widget has a real parent, showing it will raise the parent window
	// (on Windows,) which is undesirable.
	connect(
		m_parentWidget, &QWidget::destroyed, this, &PopupMessage::deleteLater);
}

void PopupMessage::setMessage(const QString &message)
{
	static const int MAX_LINES = 6;
	if(!isVisible()) {
		m_doc->clear();
	}

	QTextCursor cursor(m_doc);
	cursor.movePosition(QTextCursor::End);
	if(cursor.position() > 0) {
		cursor.insertBlock();
	}

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
static constexpr int PADDING = 4;
static constexpr int ARROW_H = 10;
}

void PopupMessage::showMessage(const QPoint &point, const QString &message)
{
	setMessage(message);

	resize(m_doc->size().toSize() + QSize(PADDING * 2, PADDING * 2 + ARROW_H));

	QRect rect(point - QPoint(width() - width() / 6, height()), size());

	const QRect screen =
		compat::widgetScreen(*m_parentWidget)->availableGeometry();

	// Make sure the popup fits horizontally
	if(rect.x() + rect.width() > screen.x() + screen.width()) {
		rect.moveRight(screen.x() + screen.width() - 1);
	} else if(rect.x() < screen.x()) {
		rect.moveLeft(screen.x());
	}
	m_arrowoffset = point.x() - rect.x();

	// Make sure the popup fits vertically
	if(rect.y() + rect.height() > screen.y() + screen.height()) {
		rect.moveBottom(screen.y() + screen.height() - 1);
	} else if(rect.y() < screen.y()) {
		rect.moveTop(screen.y());
	}

	// Redraw the background and show on screen
	redrawBubble();
	move(rect.topLeft());
	if(isVisible()) {
		update();
	} else {
		show();
	}
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
	const qreal w = contentsRect().width() - 1;
	const qreal h = contentsRect().height();
	const qreal rad = 4;
	const qreal h1 = h - ARROW_H;
	const qreal aw = 10;
	qreal arrowsafe = m_arrowoffset;
	if(arrowsafe - aw < rad) {
		arrowsafe = rad + aw;
	} else if(arrowsafe + aw > w - rad) {
		arrowsafe = w - rad - aw;
	}

	m_bubble = QPainterPath(QPointF(w - rad, 0));
	m_bubble.cubicTo(w - rad / 2, 0, w, rad / 2, w, rad);
	m_bubble.lineTo(w, h1 - rad);
	m_bubble.cubicTo(w, h1 - rad / 2, w - rad / 2, h1, w - rad, h1);

	m_bubble.lineTo(arrowsafe + aw, h1);
	m_bubble.lineTo(arrowsafe, h);
	m_bubble.lineTo(arrowsafe - aw, h1);

	m_bubble.lineTo(rad, h1);
	m_bubble.cubicTo(rad / 2, h1, 0, h1 - rad / 2, 0, h1 - rad);
	m_bubble.lineTo(0, rad);
	m_bubble.cubicTo(0, rad / 2, rad / 2, 0, rad, 0);
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
