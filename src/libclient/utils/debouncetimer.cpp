// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/utils/debouncetimer.h"

DebounceTimer::DebounceTimer(int delayMs, QObject *parent)
	: QObject{parent}
	, m_type{Type::NONE}
	, m_delayMs{delayMs}
	, m_timerId{0}
	, m_value{}
{
}

void DebounceTimer::setInt(int value)
{
	m_type = Type::INT;
	m_value = value;
	if(m_timerId != 0) {
		killTimer(m_timerId);
	}
	m_timerId = startTimer(m_delayMs);
}

void DebounceTimer::timerEvent(QTimerEvent *)
{
	killTimer(m_timerId);
	m_timerId = 0;
	switch(m_type) {
	case Type::NONE:
		qWarning("No debounce timer type set");
		break;
	case Type::INT:
		emit intChanged(m_value.toInt());
		break;
	}
	m_value.clear();
}
