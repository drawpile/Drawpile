/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

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
#ifndef CHATLINEEDIT_H
#define CHATLINEEDIT_H

#include <QStringList>
#include <QPlainTextEdit>

/**
 * @brief A specialized line edit widget for chatting, with history
  */
class ChatLineEdit : public QPlainTextEdit
{
Q_OBJECT
public:
	explicit ChatLineEdit(QWidget *parent = 0);

	//! Push text to history
	void pushHistory(const QString& text);

	//! Get the current text with trailing whitespace removed
	QString trimmedText() const;

signals:
	void returnPressed(const QString &text);

public slots:

protected:
	void keyPressEvent(QKeyEvent *event);

private:
	QStringList _history;
	QString _current;
	int _historypos;
};

#endif
