/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2019 Calle Laakkonen

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

namespace protocol { class MessagePtr; }
namespace canvas { class UserListModel; }

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
	explicit ChatBox(QWidget *parent=nullptr);

	//! Focus the text input widget
	void focusInput();

	void setUserList(canvas::UserListModel *userlist);

public slots:
	/**
	 * @brief Set default message preservation mode
	 *
	 * This sets a visual cue that informs the user whether chat messages are preserved
	 * in the session history or not
	 *
	 */
	void setPreserveMode(bool preservechat);

	//! Display a received message
	void receiveMessage(const protocol::MessagePtr &msg);

	//! Display a received marker
	void receiveMarker(int id, const QString &message);

	//! Display a system message
	void systemMessage(const QString& message, bool isAlert=false);

	void userJoined(int id, const QString &name);
	void userParted(int id);
	void kicked(const QString &kickedBy);

	//! Empty the chat box
	void clear();

	//! Initialize the chat box for a new server
	void loggedIn(int myId);

	//! Open a private chat view with this user
	void openPrivateChat(int userId);

private slots:
	void sendMessage(const QString &msg);
	void chatTabSelected(int index);
	void chatTabClosed(int index);
	void showChatContextMenu(const QPoint &pos);
	void setCompactMode(bool compact);

signals:
	void message(const protocol::MessagePtr &msg);
	void expanded(bool isVisible);

protected:
	void resizeEvent(QResizeEvent *event);

private:
	struct Private;
	Private *d;
};

}

#endif

