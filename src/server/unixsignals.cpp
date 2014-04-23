/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

#include "unixsignals.h"

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
		{SIGTERM, "sigTerm"}
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
		::write(SIG_SOCKETS[0], &a, sizeof(a));
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
	::read(SIG_SOCKETS[1], &sig, sizeof(sig));

	SignalMapping *sm = getSignalByCode(sig);
	Q_ASSERT(sm);
	if(sm) {
		// Emit corresponding signal
		QMetaObject::invokeMethod(this, sm->name);
	} else {
		qWarning("Unhandled signal!");
	}

	SIG_NOTIFIER->setEnabled(true);
}
