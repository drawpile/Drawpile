// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_CLICKEVENTFILTER_H
#define LIBCLIENT_UTILS_CLICKEVENTFILTER_H
#include <QObject>

class ClickEventFilter final : public QObject {
	Q_OBJECT
public:
	explicit ClickEventFilter(QObject *parent = nullptr);

	bool eventFilter(QObject *watched, QEvent *event) override;

Q_SIGNALS:
	void clicked();

private:
	bool m_pressed = false;
};

#endif
