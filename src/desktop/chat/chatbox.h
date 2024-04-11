// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CHATBOX_H
#define CHATBOX_H
#include <QWidget>

class Document;
class QListView;
class QPushButton;

namespace canvas {
class CanvasModel;
}

namespace net {
class Message;
}

namespace widgets {

class ChatWidget;
class GroupedToolButton;
class UserItemDelegate;

/**
 * Chat box with user list
 */
class ChatBox final : public QWidget {
	Q_OBJECT
public:
	ChatBox(Document *doc, bool smallScreenMode, QWidget *parent = nullptr);

	void setActions(QAction *inviteAction, QAction *sessionSettingsAction);
	void setSmallScreenMode(bool smallScreenMode);

	//! Focus the text input widget
	void focusInput();

	bool isCollapsed() const
	{
		return m_state == State::Collapsed || !isVisible();
	}

public slots:
	void setCurrentLayer(int layerId);

private slots:
	void onCanvasChanged(canvas::CanvasModel *canvas);
	void onServerLogin();
	void onCompatibilityModeChanged(bool compatibilityMode);
	void detachFromParent();
	void reattachToParent();

signals:
	//! User has written a new message
	void message(const net::Message &msg);

	//! Request information dialog about this user
	void requestUserInfo(int userId);
	void requestCurrentBrush(int userId);

	//! The chatbox was either expanded or collapsed
	void expandedChanged(bool isExpanded);

	//! Request that the chatbox be expanded
	void expandPlease();

	//! Detached chat box should be re-attached and reparented (or it will be
	//! destroyed)
	void reattachNowPlease();

	void muteChanged(bool muted);

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	enum class State { Expanded, Collapsed, Detached };

	ChatWidget *m_chatWidget;
	UserItemDelegate *m_userItemDelegate;
	GroupedToolButton *m_inviteButton;
	GroupedToolButton *m_sessionSettingsButton;
	GroupedToolButton *m_chatMenuButton;
	QListView *m_userList;

	State m_state = State::Expanded;
};

}

#endif
