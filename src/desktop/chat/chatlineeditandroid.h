// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CHATLINEEDITANDROID_H
#define CHATLINEEDITANDROID_H

#include <QStringList>
#include <QLineEdit>

/**
 * @brief A specialized line edit widget for chatting, compatible with Android,
 * not relying on any key presses being issued.
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
