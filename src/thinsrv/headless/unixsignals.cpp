/*
   Drawpile - a collaborative drawing program.

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

#include "thinsrv/headless/unixsignals.h"

#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>

#include <QSocketNotifier>
#include <QMetaMethod>

namespace {
	UnixSignals *SINGLETON;

	int SIG_SOCKETS[2];
	QSocketNotifier *SIG_NOTIFIER;

	struct SignalMapping {
		const int sig;
		const char *name;

		bool connected;

		SignalMapping(int _sig, const char *signame)
			: sig(_sig), name(signame), connected(false) { }
	};

	// Mapping of signal code -> UnixSignals signal name
	SignalMapping SIG_MAP[] = {
		{SIGINT, "sigInt"},
		{SIGTERM, "sigTerm"},
		{SIGUSR1, "sigUsr1"},
	};
	static const int SIGNALS = sizeof(SIG_MAP) / sizeof(SignalMapping);

	SignalMapping *getSignalByCode(int sig) {
		for(int i=0;i<SIGNALS;++i)
			if(SIG_MAP[i].sig == sig)
				return &SIG_MAP[i];
		return nullptr;
	}

	SignalMapping *getSignalByName(const QString &name) {
		for(int i=0;i<SIGNALS;++i)
			if(SIG_MAP[i].name == name)
				return &SIG_MAP[i];
		return nullptr;
	}

	void signalHandler(int sig)
	{
		uchar a = sig;
		ssize_t wb = ::write(SIG_SOCKETS[0], &a, sizeof(a));
		Q_UNUSED(wb);
	}
}

UnixSignals *UnixSignals::instance()
{
	if(!SINGLETON)
		SINGLETON = new UnixSignals;

	return SINGLETON;
}

UnixSignals::UnixSignals() : QObject()
{
	// Create socket pair to notify ourselves of the UNIX signal
	// See: http://qt-project.org/doc/qt-5/unix-signals.html
	if (::socketpair(AF_UNIX, SOCK_STREAM, 0, SIG_SOCKETS)==0) {
		SIG_NOTIFIER = new QSocketNotifier(SIG_SOCKETS[1], QSocketNotifier::Read, this);
		connect(SIG_NOTIFIER, SIGNAL(activated(int)), this, SLOT(handleSignal()));
	} else {
		qWarning("Couldn't set up signal socket pair!");
	}
}

void UnixSignals::connectNotify(const QMetaMethod &signal)
{
	if(!SIG_NOTIFIER)
		return;

	SignalMapping *sm = getSignalByName(signal.name());
	if(sm && !sm->connected) {
		// Automatically set up the UNIX signal handler when someone connects to the Qt signal
		sm->connected = true;

		struct sigaction sa;

		sa.sa_handler = signalHandler;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;

		sigaction(sm->sig, &sa, 0);
	}
}

void UnixSignals::handleSignal()
{
	SIG_NOTIFIER->setEnabled(false);

	uchar sig;
	ssize_t rb = ::read(SIG_SOCKETS[1], &sig, sizeof(sig));
	if(rb != 1 ) {
		qWarning("read from signal socket failed!");

	} else {
		SignalMapping *sm = getSignalByCode(sig);
		Q_ASSERT(sm);
		if(sm) {
			// Emit corresponding signal
			QMetaObject::invokeMethod(this, sm->name);
		} else {
			qWarning("Unhandled signal!");
		}
	}

	SIG_NOTIFIER->setEnabled(true);
}
