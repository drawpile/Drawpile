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
#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>

class QTextBrowser;
class ChatLineEdit;

namespace widgets {

/**
 * @brief Chat window
 *
 * A widget for chatting with other users
 */
class ChatBox: public QWidget
{
Q_OBJECT
public:
	explicit ChatBox(QWidget *parent=0);

	//! Focus the text input widget
	void focusInput();

public slots:
	//! Display a received message
	void receiveMessage(const QString& nick, const QString &message, bool announcement, bool isme=false);

	//! Display a received marker
	void receiveMarker(const QString &nick, const QString &message);

	//! Display a system message
	void systemMessage(const QString& message);

	void userJoined(int id, const QString &name);
	void userParted(const QString &name);
	void kicked(const QString &kickedBy);

	//! Empty the chat box
	void clear();

private slots:
	void sendMessage(const QString &msg);

signals:
	void message(const QString &msg, bool announcement);
	void opCommand(const QString &cmd);
	void expanded(bool isVisible);

protected:
	void resizeEvent(QResizeEvent *event);

private:
	QTextBrowser *_view;
	ChatLineEdit *_myline;
	bool _wasCollapsed;
};

}

#endif

