// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWDANCE_EVENTLOG_H
#define DRAWDANCE_EVENTLOG_H

extern "C" {
#include <dpcommon/event_log.h>
}

class QString;

namespace drawdance {

class EventLog final {
public:
	static bool open(const QString &path);
	static bool close();
	static bool isOpen();

private:
	static EventLog instance;
	~EventLog();
};

}

#endif
