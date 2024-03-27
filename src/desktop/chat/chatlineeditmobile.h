// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CHATLINEEDITMOBILE_H
#define CHATLINEEDITMOBILE_H

#include <QStringList>
#include <QLineEdit>

/**
 * @brief A specialized line edit widget for chatting, compatible with mobile
 * devices, not relying on any key presses being issued. Otherwise pressing
 * return on the soft keyboard just ends up inputting a newline.
 */
class ChatLineEdit final : public QLineEdit
{
Q_OBJECT
public:
	explicit ChatLineEdit(QWidget *parent = nullptr);

signals:
	void messageSent(const QString &message);

private slots:
	void sendMessage();
};

#endif
