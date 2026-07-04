// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_COMPRESSEDTIMER_H
#define LIBCLIENT_UTILS_COMPRESSEDTIMER_H
#include <QObject>
#include <QTimer>

class CompressedTimer final : public QObject {
	Q_OBJECT
public:
	explicit CompressedTimer(
		Qt::TimerType timerType, int msec, QObject *parent = nullptr);

	void start();
	void stop();

Q_SIGNALS:
	void timeout();

private:
	void checkTimeout();

	QTimer m_timer;
	int m_again = false;
};

#endif
