/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

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
#ifndef LOCALSERVER_H
#define LOCALSERVER_H

#include <QProcess>

//! Local server control
/**
 * The LocalServer controls a drawpile server process.
 */
class LocalServer : public QObject {
	Q_OBJECT
	public:
		~LocalServer();

		//! Get the global LocalServer instance
		static LocalServer *getInstance();

		//! Get the address of the local server
		static QString address();

		//! Make sure server is running on the port set in settings
		bool ensureRunning();

		//! Make sure server is running on the given port
		bool ensureRunning(int port);

		//! Get some explanation on why the server didn't start
		QString errorString() const;

		//! Read server output
		QString serverOutput();

		//! Was the local server binary found
		bool isAvailable() const { return available_; }

	signals:
		void serverCrashed();

	public slots:
		//! Shutdown the server
		void shutdown();

	private slots:
		void serverFinished(int exitcode, QProcess::ExitStatus exitstatus);

	private:
		LocalServer();

		QString binpath_;
		QProcess server_;
		int port_;
		bool noerror_;
		bool available_;
};

#endif

