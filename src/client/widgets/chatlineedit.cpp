/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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
#include <QKeyEvent>

#include "chatlineedit.h"

ChatLineEdit::ChatLineEdit(QWidget *parent) :
	QLineEdit(parent), _historypos(0)
{
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
				_current = text();
			--_historypos;
			setText(_history[_historypos]);
		}
	} else if(event->key() == Qt::Key_Down) {
		if(_historypos<_history.count()-1) {
			++_historypos;
			setText(_history[_historypos]);
		} else if(_historypos==_history.count()-1) {
			++_historypos;
			setText(_current);
		}
	} else if(event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
		QString txt = text().trimmed();
		if(!txt.isEmpty()) {
			pushHistory(txt);
			_historypos = _history.count();
			setText(QString());
			emit returnPressed(txt);
		}
	} else {
		QLineEdit::keyPressEvent(event);
	}
}

bool ChatLineEdit::isEmpty() const
{
	return text().trimmed().isEmpty();
}
