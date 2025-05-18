// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/debouncetimer.h"

DebounceTimer::DebounceTimer(int delayMs, QObject *parent)
	: QObject{parent}
	, m_type{Type::None}
	, m_delayMs{delayMs}
	, m_timerId{0}
	, m_value{}
{
}

void DebounceTimer::setNone()
{
	m_type = Type::None;
	restartTimer();
}

void DebounceTimer::setInt(int value)
{
	m_type = Type::Int;
	m_value = value;
	restartTimer();
}

bool DebounceTimer::stopTimer()
{
	if(m_timerId != 0) {
		killTimer(m_timerId);
		m_timerId = 0;
		return true;
	} else {
		return false;
	}
}

void DebounceTimer::timerEvent(QTimerEvent *)
{
	stopTimer();
	switch(m_type) {
	case Type::None:
		emit noneChanged();
		break;
	case Type::Int:
		emit intChanged(m_value.toInt());
		break;
	}
	m_value.clear();
}

void DebounceTimer::restartTimer()
{
	stopTimer();
	m_timerId = startTimer(m_delayMs);
}
