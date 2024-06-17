// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QBitmap>
#include <QPainter>
#include <QScreen>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>

#include "desktop/utils/qtguicompat.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/popupmessage.h"

// On Windows, giving this widget a real parent causes the main window to get
// raised along with the notification being shown, which we don't want. On
// Wayland on the other hand, the widget must have a parent or else it can't be
// positioned because Wayland hates the living. On X11, not giving it a parent
// causes some weird effects, like the notification showing up on wrong
// desktops. No clue what it does on macOS. So were special-casing Windows for
// now and hope that the other systems behave somewhat sensibly.
#ifdef Q_OS_WIN
#	define POPUP_MESSAGE_PARENT_ARG nullptr
#else
#	define POPUP_MESSAGE_PARENT_ARG parent
#endif

namespace widgets {

PopupMessage::PopupMessage(QWidget *parent)
	: QWidget{POPUP_MESSAGE_PARENT_ARG, Qt::ToolTip | Qt::FramelessWindowHint}
	, m_arrowoffset{0}
	, m_timer{new QTimer{this}}
	, m_doc{new QTextDocument{this}}
#ifdef Q_OS_WIN
	, m_parentWidget{parent}
#endif
{
	setAttribute(Qt::WA_TranslucentBackground);
	m_timer->setSingleShot(true);
	m_timer->setInterval(2500);

	connect(m_timer, &QTimer::timeout, this, &PopupMessage::hide);

#ifdef Q_OS_WIN
	connect(
		m_parentWidget, &QWidget::destroyed, this, &PopupMessage::deleteLater);
#endif
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

	cursor.insertText(message);

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

#ifdef Q_OS_WIN
	QWidget *pw = m_parentWidget;
#else
	QWidget *pw = parentWidget();
#endif
	QRect rect = utils::moveRectToFit(
		QRect(point - QPoint(width() - width() / 6, height()), size()),
		compat::widgetScreen(*pw)->availableGeometry());
	m_arrowoffset = point.x() - rect.x();

	// Redraw the background and show on screen
	redrawBubble();
	move(rect.topLeft());
	if(isVisible()) {
		update();
	} else {
		show();
		raise();
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
