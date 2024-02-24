// SPDX-License-Identifier: GPL-3.0-or-later

#include "thinsrv/initsys.h"

#include <systemd/sd-daemon.h>
#include <sys/socket.h>
#include <cstdio>

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
	sd_notifyf(0, "STATUS=%s", status.toLocal8Bit().constData());
}

QList<int> getListenFds()
{
	QList<int> fds;
	int n = sd_listen_fds(0);

	for(int i=0;i<n;++i) {
		int fd = SD_LISTEN_FDS_START + i;
		if(sd_is_socket_inet(fd, AF_UNSPEC, SOCK_STREAM, 1, 0) < 0) {
			qCritical("Socket %d is not a listening inet socket!", i);
		} else {
			fds.append(fd);
		}
	}

	return fds;
}

}
