// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/chat/chatlineedit.h"
#include "desktop/utils/widgetutils.h"
#include <QKeyEvent>
#include <QScopedValueRollback>
#include <QScrollBar>

ChatLineEdit::ChatLineEdit(QWidget *parent)
	: QPlainTextEdit(parent)
{
	m_kineticScroller = utils::bindKineticScrollingWith(
		this, Qt::ScrollBarAlwaysOff, Qt::ScrollBarAlwaysOff);
	connect(
		verticalScrollBar(), &QAbstractSlider::valueChanged, this,
		&ChatLineEdit::fixScroll);
}

void ChatLineEdit::pushHistory(const QString &text)
{
	// Disallow consecutive duplicates
	if(!m_history.isEmpty() && m_history.last() == text) {
		return;
	}

	// Add to history list while limiting its size
	m_history.append(text);
	if(m_history.count() > 50) {
		m_history.removeFirst();
	}

	++m_historypos;
}

void ChatLineEdit::keyPressEvent(QKeyEvent *event)
{
	if(event->key() == Qt::Key_Up) {
		if(m_historypos > 0) {
			if(m_historypos == m_history.count())
				m_current = toPlainText();
			--m_historypos;
			setPlainText(m_history[m_historypos]);
		}
	} else if(event->key() == Qt::Key_Down) {
		if(m_historypos < m_history.count() - 1) {
			++m_historypos;
			setPlainText(m_history[m_historypos]);
		} else if(m_historypos == m_history.count() - 1) {
			++m_historypos;
			setPlainText(m_current);
		}
	} else if(
		(event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) &&
		!(event->modifiers() & Qt::ShiftModifier)) {
		QString txt = toPlainText();
		if(!txt.trimmed().isEmpty()) {
			pushHistory(txt);
			m_historypos = m_history.count();
			setPlainText(QString());
			emit messageSent(txt);
		}
	} else {
		QPlainTextEdit::keyPressEvent(event);
	}

	resizeBasedOnLines();
}

void ChatLineEdit::resizeEvent(QResizeEvent *)
{
	// Line height depends on widget margins, which change after constructor is
	// called.
	resizeBasedOnLines();
}

void ChatLineEdit::fixScroll(int value)
{
	fixScrollAt(value, int(document()->size().height()));
}

void ChatLineEdit::resizeBasedOnLines()
{
	int lineCount = int(document()->size().height());
	int clampedLineCount = qBound(1, lineCount, 5);
	setFixedHeight(lineCountToWidgetHeight(clampedLineCount));
	// Scrollbar shows up sometimes for no reason, hardcode to hide it when
	// in autosize range. It'll also scroll down and hide the top line for
	// no reason.
	m_kineticScroller->setVerticalScrollBarPolicy(
		lineCount <= 5 ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAlwaysOn);
	fixScrollAt(verticalScrollBar()->value(), lineCount);
}

// Based on
// https://github.com/cameel/auto-resizing-text-edit/blob/master/auto_resizing_text_edit/auto_resizing_text_edit.py
int ChatLineEdit::lineCountToWidgetHeight(int lineCount) const
{
	Q_ASSERT(lineCount > 0);

	QMargins widgetMargins = contentsMargins();
	qreal documentMargin = document()->documentMargin();
	QFontMetrics fontMetrics(document()->defaultFont());

	return (
		widgetMargins.top() + documentMargin +
		lineCount * fontMetrics.lineSpacing() + documentMargin +
		widgetMargins.bottom());
}

void ChatLineEdit::fixScrollAt(int value, int lineCount)
{
	if(!m_fixingScroll && lineCount <= 5 && value != 0) {
		QScopedValueRollback<bool> guard{m_fixingScroll, true};
		QScrollBar *vbar = verticalScrollBar();
		vbar->setValue(0);
	}
}
