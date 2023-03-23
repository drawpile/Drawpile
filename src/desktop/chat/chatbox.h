// SPDX-License-Identifier: GPL-3.0-or-later

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

	//! Request information dialog about this user
	void requestUserInfo(int userId);

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

