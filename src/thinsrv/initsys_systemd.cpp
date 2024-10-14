// SPDX-License-Identifier: GPL-3.0-or-later
#include "thinsrv/initsys.h"
#include <cinttypes>
#include <cstdio>
#include <sys/socket.h>
#include <systemd/sd-daemon.h>

namespace initsys {

QString name()
{
	return QStringLiteral("systemd");
}

void notifyReady()
{
	sd_notify(0, "READY=1");
}

void notifyStatus(const QString &status)
{
	sd_notifyf(0, "STATUS=%s", qUtf8Printable(status));
}

QList<int> getListenFds()
{
	QList<int> fds;
	int n = sd_listen_fds(0);

	for(int i = 0; i < n; ++i) {
		int fd = SD_LISTEN_FDS_START + i;
		if(sd_is_socket_inet(fd, AF_UNSPEC, SOCK_STREAM, 1, 0) < 0) {
			qCritical("Socket %d is not a listening inet socket!", i);
		} else {
			fds.append(fd);
		}
	}

	return fds;
}

int getWatchdogMsec()
{
	uint64_t usec;
	int result = sd_watchdog_enabled(1, &usec);
	if(result < 0) {
		qWarning("sd_watchdog_enabled: error %d", result);
		return -1;
	} else if(result == 0) {
		return 0;
	} else {
		// The systemd docs recommend sending keepalives every half interval.
		uint64_t msec = usec / uint64_t(2000);
		if(msec == uint64_t(0)) {
			qWarning(
				"Watchdog timeout %" PRIu64
				"usec results in keepalive interval of 0",
				usec);
			return -2;
		} else if(msec > uint64_t(60000)) {
			qWarning(
				"Excessive watchdog timeout of %" PRIu64
				"usec, using 1 minute keepalive interval instead",
				usec);
			return 60000;
		} else {
			return int(msec);
		}
	}
}

void watchdog()
{
	sd_notify(0, "WATCHDOG=1");
}

}
