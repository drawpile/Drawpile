/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

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
#ifndef NETWORK_H
#define NETWORK_H

#include <QThread>

namespace protocol {
	class Message;
}

//! Client network classes
/**
 */
namespace network {

class NetworkPrivate;

//! Network connection handling thread
/**
 * The network thread object handles the reception and transmission of
 * messages defined in the protocol.
 *
 * Messages can be sent using the \a send function.
 * The signal \a received is emitted when new messages have become available
 * and can be read with \a receive.
 */
class Connection : public QThread {
	Q_OBJECT
	public:
		Connection(QObject *parent=0);
		~Connection();

		//! Connect to host
		void connectHost(const QString& host, quint16 port);

		//! Disconnect from host
		void disconnectHost();

		//! Send a message
		void send(protocol::Message *msg);

		//! Receive a message
		protocol::Message *receive();

	signals:
		//! Connection established
		void connected();

		//! Connection cut
		void disconnected(const QString& message);

		//! One or more message available in receive buffer
		void received();

		//! All messages in send buffer have been transmitted
		void sent();

		//! An error occured
		void error(const QString& message);

	protected:
		void run();

	private:
		QString host_;
		quint16 port_;
		NetworkPrivate *p_;
};

}

#endif

