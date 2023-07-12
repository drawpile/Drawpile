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
class QPushButton;

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

	void setActions(QAction *hostAction, QAction *joinAction, QAction *browseAction, QAction *inviteAction);

	//! Focus the text input widget
	void focusInput();

	bool isCollapsed() const { return m_state == State::Collapsed; }

public slots:
	void setConnected(bool inviteEnabled);

private slots:
	void onCanvasChanged(canvas::CanvasModel *canvas);
	void onServerLogin();
	void onCompatibilityModeChanged(bool compatibilityMode);
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
	QPushButton *m_hostButton;
	QPushButton *m_joinButton;
	QPushButton *m_browseButton;
	QPushButton *m_inviteButton;
	QListView *m_userList;

	State m_state = State::Expanded;
};

}

#endif

