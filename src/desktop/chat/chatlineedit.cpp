// SPDX-License-Identifier: GPL-3.0-or-later

#include <QKeyEvent>
#include <QScrollBar>

#include "desktop/chat/chatlineedit.h"

ChatLineEdit::ChatLineEdit(QWidget *parent) :
	QPlainTextEdit(parent), _historypos(0)
{
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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
		QString txt = trimmedText();
		if(!txt.isEmpty()) {
			pushHistory(txt);
			_historypos = _history.count();
			setPlainText(QString());
			emit returnPressed(txt);
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

QString ChatLineEdit::trimmedText() const
{
	QString str = toPlainText();

	// Remove trailing whitespace
	int chop = str.length();
	while(chop>0 && str.at(chop-1).isSpace())
		--chop;

	if(chop==0)
		return QString();
	str.truncate(chop);

	return str;
}

void ChatLineEdit::resizeBasedOnLines() 
{
	int lineCount = int(document()->size().height());
	int clampedLineCount = qBound(1, lineCount, 5);
	setFixedHeight(lineCountToWidgetHeight(clampedLineCount));

	// Scrollbar shows up sometimes for no reason, hardcode to hide it when in autosize range.
	// It'll also scroll down and hide the top line for no reason.
	if(lineCount <= 5) {
		verticalScrollBar()->setValue(0);
		setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	}
	else {
		setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	}
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
