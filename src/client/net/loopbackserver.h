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
#ifndef DP_NET_LOOPBACKSERVER_H
#define DP_NET_LOOPBACKSERVER_H

#include <QObject>

#include "server.h"

#include "../shared/util/idlist.h"

/*
 * Uncomment this to enable network lag simulator.
 * The number is the maximum amount of lag in milliseconds to inject between messages.
 */
//#define LAG_SIMULATOR 50

class QTimer;

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
signals:
	void messageReceived(protocol::MessagePtr message);
	
private slots:
#ifdef LAG_SIMULATOR
	void sendDelayedMessage();
#endif

private:
	UsedIdList _layer_ids;
	UsedIdList _annotation_ids;
#ifdef LAG_SIMULATOR
	QTimer *_lagtimer;
	QList<protocol::MessagePtr> _msgqueue;
#endif
};


}

#endif

