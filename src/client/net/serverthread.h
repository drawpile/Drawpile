/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2013 Calle Laakkonen

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

    /**
     * @brief Set the default port to listen on
     *
     * This should be called before startServer(). If
     * the port is unavailable, a random port will be selected.
     * @param port
     */
    void setPort(int port) { _port = port; }

	/**
	 * @brief Start the thread and wait for the server to start
	 * @param title the title of the server (used for DNSSD announcement)
	 * @return the port the server is listening on or 0 in case of error
	 */
	int startServer(const QString &title);

	/**
	 * @brief Is the server listening on the default port
	 * @return true if port is Drawpile's default
	 * @pre startServer() returned nonzero
	 */
	bool isOnDefaultPort() const;

	/**
	 * @brief Get the error message
	 *
	 * The error message is available if startServer() returned 0.
	 */
	const QString &errorString() const { return _error; }

	/**
	 * @brief Automatically self delete when server exists
	 */
	void setDeleteOnExit() { _deleteonexit = true; }

protected:
	void run();

private:
	bool _deleteonexit;

	int _port;
	QString _error;
	QMutex _startmutex;
	QWaitCondition _starter;
};

}

#endif
