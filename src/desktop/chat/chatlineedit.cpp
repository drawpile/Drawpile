// SPDX-License-Identifier: GPL-3.0-or-later

#include <QKeyEvent>
#include <QScrollBar>
#include <QScopedValueRollback>

#include "desktop/chat/chatlineedit.h"
#include "desktop/utils/widgetutils.h"

ChatLineEdit::ChatLineEdit(QWidget *parent) :
	QPlainTextEdit(parent), _historypos(0), _fixingScroll(false),
	_kineticScrollBarsHidden(utils::isKineticScrollingBarsHidden())
{
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	utils::initKineticScrolling(this);
	connect(
		verticalScrollBar(), &QAbstractSlider::valueChanged, this,
		&ChatLineEdit::fixScroll);
}

void ChatLineEdit::pushHistory(const QString &text)
{
	// Disallow consecutive duplicates
	if(!_history.isEmpty() && _history.last() == text)
		return;

	// Add to history list while limiting its size
	_history.append(text);
	if(_history.count() > 50)
		_history.removeFirst();

	++_historypos;
}

void ChatLineEdit::keyPressEvent(QKeyEvent *event)
{
	if(event->key() == Qt::Key_Up) {
		if(_historypos>0) {
			if(_historypos==_history.count())
				_current = toPlainText();
			--_historypos;
			setPlainText(_history[_historypos]);
		}
	} else if(event->key() == Qt::Key_Down) {
		if(_historypos<_history.count()-1) {
			++_historypos;
			setPlainText(_history[_historypos]);
		} else if(_historypos==_history.count()-1) {
			++_historypos;
			setPlainText(_current);
		}
	} else if((event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) && !(event->modifiers() & Qt::ShiftModifier)) {
		QString txt = toPlainText();
		if(!txt.trimmed().isEmpty()) {
			pushHistory(txt);
			_historypos = _history.count();
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
	// Line height depends on widget margins, which change after constructor is called.
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

	if(!_kineticScrollBarsHidden) {
		// Scrollbar shows up sometimes for no reason, hardcode to hide it when
		// in autosize range. It'll also scroll down and hide the top line for
		// no reason.
		setVerticalScrollBarPolicy(
			lineCount <= 5 ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAlwaysOn);
	}
	fixScrollAt(verticalScrollBar()->value(), lineCount);
}

// Based on https://github.com/cameel/auto-resizing-text-edit/blob/master/auto_resizing_text_edit/auto_resizing_text_edit.py
int ChatLineEdit::lineCountToWidgetHeight(int lineCount) const
{
	Q_ASSERT(lineCount > 0);

    QMargins widgetMargins  = contentsMargins();
    qreal documentMargin = document()->documentMargin();
    QFontMetrics fontMetrics(document()->defaultFont());

    return (
		widgetMargins.top() +
		documentMargin +
		lineCount * fontMetrics.lineSpacing() +
		documentMargin +
		widgetMargins.bottom()
    );
}

void ChatLineEdit::fixScrollAt(int value, int lineCount)
{
	if(!_fixingScroll && lineCount <= 5 && value != 0) {
		QScopedValueRollback<bool> guard{_fixingScroll, true};
		QScrollBar *vbar = verticalScrollBar();
		vbar->setValue(0);
	}
}
