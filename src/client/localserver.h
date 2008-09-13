/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2008 Calle Laakkonen

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
#include <QThread>

#ifndef LOCALSERVER_H
#define LOCALSERVER_H

namespace server {
	class Server;
}

//! Local server control
/**
 * The LocalServer provides a singleton thread that runs the DrawPile server.
 */
class LocalServer : public QThread {
	Q_OBJECT
	public:
		~LocalServer() { }

		//! Get the address of the local server
		static QString address();

		//! Start the server
		static bool startServer();

		//! Stop the server
		static void stopServer();

	protected:
		void run();

	private:
		LocalServer();

		static LocalServer *instance_;

		server::Server *server_;
		bool noerror_;
};

#endif

