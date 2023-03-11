/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019-2021 Calle Laakkonen

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
#ifndef CHATBOX_H
#define CHATBOX_H

#include <QWidget>

namespace canvas {
	class CanvasModel;
}

namespace drawdance {
	class Message;
}

class QListView;

class Document;

namespace widgets {

class ChatWidget;
class UserItemDelegate;

/**
 * Chat box with user list
 */
class ChatBox final : public QWidget
{
	Q_OBJECT
public:
	explicit ChatBox(Document *doc, QWidget *parent=nullptr);

	//! Focus the text input widget
	void focusInput();

	bool isCollapsed() const { return m_state == State::Collapsed; }

private slots:
	void onCanvasChanged(canvas::CanvasModel *canvas);
	void onServerLogin();
	void detachFromParent();
	void reattachToParent();

signals:
	//! User has written a new message
	void message(const drawdance::Message &msg);

	//! The chatbox was either expanded or collapsed
	void expandedChanged(bool isExpanded);

	//! Request that the chatbox be expanded
	void expandPlease();

	//! Detached chat box should be re-attached and reparented (or it will be destroyed)
	void reattachNowPlease();

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	enum class State {
		Expanded,
		Collapsed,
		Detached
	};

	ChatWidget *m_chatWidget;
	UserItemDelegate *m_userItemDelegate;
	QListView *m_userList;

	State m_state = State::Expanded;
};

}

#endif

