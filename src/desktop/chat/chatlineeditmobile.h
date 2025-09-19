// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_CHAT_CHATLINEEDITMOBILE_H
#define DESKTOP_CHAT_CHATLINEEDITMOBILE_H
#include <QLineEdit>
#include <QStringList>

/**
 * @brief A specialized line edit widget for chatting, compatible with mobile
 * devices, not relying on any key presses being issued. Otherwise pressing
 * return on the soft keyboard just ends up inputting a newline.
 */
class ChatLineEdit final : public QLineEdit {
	Q_OBJECT
public:
	explicit ChatLineEdit(QWidget *parent = nullptr);

	void sendMessage();

signals:
	void messageAvailable(bool available);
	void messageSent(const QString &message);

private:
	void updateMessageAvailable();

	bool m_wasMessageAvailable = false;
};

#endif
