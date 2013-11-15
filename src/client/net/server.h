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

namespace protocol {
	class Message;
}

namespace net {

/**
 * \brief Abstract base class for servers
 */
class Server {
public:
	virtual ~Server() = default;
	
	/**
	 * \brief Send a message to the server
	 */
	virtual void sendMessage(protocol::Message *msg) = 0;
};


}

#endif

