// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CHATWIDGET_H
#define CHATWIDGET_H
#include "desktop/notifications.h"
#include <QWidget>

class QMenu;

namespace canvas {
class UserListModel;
}

namespace net {
class Message;
}

namespace widgets {

/**
 * @brief Chat window
 *
 * A widget for chatting with other users
 */
class ChatWidget final : public QWidget {
	Q_OBJECT
public:
	explicit ChatWidget(QWidget *parent = nullptr);
	~ChatWidget() override;

	void focusInput();
	void setAttached(bool isAttached);
	void setUserList(canvas::UserListModel *userlist);

	QMenu *externalMenu();

public slots:
	/**
	 * @brief Set default message preservation mode
	 *
	 * This sets a visual cue that informs the user whether chat messages are
	 * preserved in the session history or not
	 *
	 */
	void setPreserveMode(bool preservechat);

	//! Display a received message
	void receiveMessage(
		int sender, int recipient, uint8_t tflags, uint8_t oflags,
		const QString &message);

	//! Display a system message
	void systemMessage(const QString &message, bool isAlert = false);

	//! Set the message pinned to the top of the chat box
	void setPinnedMessage(const QString &message);

	void userJoined(int id, const QString &name);
	void userParted(int id);

	//! Empty the chat box
	void clear();

	//! Initialize the chat box for a new server
	void loggedIn(int myId);

	//! Open a private chat view with this user
	void openPrivateChat(int userId);

protected:
	void resizeEvent(QResizeEvent *) override;

private slots:
	void scrollBarMoved(int);
	void sendMessage(const QString chatMessage);
	void chatTabSelected(int index);
	void chatTabClosed(int index);
	void showChatContextMenu(const QPoint &pos);
	void contextMenuAboutToShow();
	void setCompactMode(bool compact);
	void attach();
	void setMentionEnabled(bool enabled);
	void setMentionTriggerList(const QString &triggerList);

signals:
	void message(const net::Message &msg);
	void detachRequested();
	void expandRequested();
	void muteChanged(bool muted);

private:
#ifdef Q_OS_ANDROID
	// On Android, image resources don't work, leading to avatars not showing.
	// We'll just force the use of compact mode, since that doesn't use avatars.
	static constexpr bool COMPACT_ONLY = true;
	// Detaching the chat window in a single-window environment is a bad idea.
	static constexpr bool ALLOW_DETACH = false;
#else
	static constexpr bool COMPACT_ONLY = false;
	static constexpr bool ALLOW_DETACH = true;
#endif

	void setMentionUsername(const QString &username);
	bool isMention(const QString &message);
	static QString makeMentionPattern(const QString &trigger);

	void notifySanitize(notification::Event event, const QString &message);
	void notify(notification::Event event, const QString &message);

	struct Private;
	Private *d;
};

}

#endif
