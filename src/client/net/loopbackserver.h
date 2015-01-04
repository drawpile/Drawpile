/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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
#ifndef DP_NET_LOOPBACKSERVER_H
#define DP_NET_LOOPBACKSERVER_H

#include <QObject>

#include "server.h"

#include "../shared/util/idlist.h"

namespace net {

/**
 * \brief Loopback sever for local single user mode
 */
class LoopbackServer : public QObject, public Server
{
	Q_OBJECT
public:
	explicit LoopbackServer(QObject *parent=0);
	
	//! Reset tracked state
	void reset();
	
	/**
	 * \brief Send a message to the server
	 */
	void sendMessage(protocol::MessagePtr msg);

	void sendSnapshotMessages(QList<protocol::MessagePtr> msgs);

	void logout();

	void pauseInput(bool pause);

signals:
	void messageReceived(protocol::MessagePtr message);
	
private:
	UsedIdList _layer_ids;
	UsedIdList _annotation_ids;
};


}

#endif

