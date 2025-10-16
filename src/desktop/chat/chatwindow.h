// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QWidget>

namespace widgets {

class ChatWindow final : public QWidget
{
	Q_OBJECT
public:
	explicit ChatWindow(QWidget *content, QWidget *parent = nullptr);

signals:
	void closing();

protected:
	void closeEvent(QCloseEvent *event) override;
};

}

#endif // CHATWINDOW_H
