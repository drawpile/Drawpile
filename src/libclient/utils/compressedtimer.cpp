// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/compressedtimer.h"

CompressedTimer::CompressedTimer(
	Qt::TimerType timerType, int msec, QObject *parent)
	: QObject(parent)
{
	m_timer.setTimerType(timerType);
	m_timer.setInterval(msec);
	m_timer.setSingleShot(false);
	connect(
		&m_timer, &QTimer::timeout, this, &CompressedTimer::checkTimeout,
		Qt::DirectConnection);
}

void CompressedTimer::start()
{
	if(m_timer.isActive()) {
		m_again = true;
	} else {
		m_again = false;
		m_timer.start();
	}
}

void CompressedTimer::stop()
{
	m_timer.stop();
}

void CompressedTimer::checkTimeout()
{
	if(m_again) {
		m_again = false;
	} else {
		m_timer.stop();
		Q_EMIT timeout();
	}
}
