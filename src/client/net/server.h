/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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
#ifndef DP_NET_SERVER_H
#define DP_NET_SERVER_H

#include "../shared/net/message.h"

namespace net {

/**
 * \brief Abstract base class for servers
 */
class Server {
    friend class LoginHandler;
public:
    Server(bool local) : _local(local) {}
	virtual ~Server() = default;
	
	/**
	 * \brief Send a message to the server
	 */
    virtual void sendMessage(protocol::MessagePtr msg) = 0;

    /**
     * @brief Enqueue messages needed to construct an initial state
     *
     * Unlike normal messages, the snapshot messages are kept in a separate queue
     * and are sent asynchronously so snapshot uploading won't entirely block this user.
     * @param msgs
     */
    virtual void sendSnapshotMessages(QList<protocol::MessagePtr> msgs) = 0;

    /**
     * @brief Is this a local server?
     * @return true if local
     */
    bool isLocal() const { return _local; }

protected:
    virtual void loginFailure(const QString &message) {}
    virtual void loginSuccess() {}

private:
    bool _local;
};


}

#endif

