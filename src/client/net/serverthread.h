/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2013 Calle Laakkonen

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
#include <QWaitCondition>
#include <QMutex>

#ifndef DP_NET_SERVERTHREAD_H
#define DP_NET_SERVERTHREAD_H

namespace net {

/**
 * @brief A thread for running a local server
 */
class ServerThread : public QThread {
Q_OBJECT
public:
	ServerThread(QObject *parent=0);

	//! Get the address of the local server
	static QString address();

	/**
	 * @brief Start the thread and wait for the server to start
	 * @return the port the server is listening on or 0 in case of error
	 */
	int startServer();

	/**
	 * @brief Is the server listening on the default port
	 * @return true if port is DrawPile's default
	 * @pre startServer() returned nonzero
	 */
	bool isOnDefaultPort() const;

	/**
	 * @brief Automatically self delete when server exists
	 */
	void setDeleteOnExit() { _deleteonexit = true; }

protected:
	void run();

private:
	bool _deleteonexit;

	int _port;
	QMutex _startmutex;
	QWaitCondition _starter;
};

}

#endif
