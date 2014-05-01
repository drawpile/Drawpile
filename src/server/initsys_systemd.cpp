/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <systemd/sd-daemon.h>
#include <sys/socket.h>

#include "initsys.h"

namespace initsys {

namespace {
void printSystemdLog(logger::LogLevel level, const QString &msg)
{
	const char *prefix;
	switch(level) {
	case logger::LOG_ERROR: prefix = SD_ERR; break;
	case logger::LOG_WARNING: prefix = SD_WARNING; break;
	case logger::LOG_DEBUG: prefix = SD_DEBUG; break;
	default: prefix = SD_INFO; break;
	}

	std::cerr << prefix << msg.toLocal8Bit().constData() << std::endl;
}
}

void setInitSysLogger()
{
	logger::setLogPrinter(printSystemdLog);
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
			logger::error() << "Socket" << i << " is not listening inet socket!";
		} else {
			fds.append(fd);
		}
	}

	return fds;
}

}
