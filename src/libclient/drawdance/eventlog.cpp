extern "C" {
#include <dpcommon/output.h>
}
#include "libclient/drawdance/eventlog.h"
#include <QString>

namespace drawdance {

bool EventLog::open(const QString &path)
{
	DP_Output *output = DP_gzip_output_new_from_path(path.toUtf8().constData());
	return output ? DP_event_log_open(output) : false;
}

bool EventLog::close()
{
	return DP_event_log_close();
}

bool EventLog::isOpen()
{
	return DP_event_log_is_open();
}

EventLog EventLog::instance{};

EventLog::~EventLog()
{
	if(DP_event_log_is_open()) {
		if(!DP_event_log_close()) {
			qWarning("Error closing event log output: %s", DP_error());
		}
	}
}

}
