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

#include <QDebug>

#include "loopbackserver.h"

#include "../shared/net/layer.h"
#include "../shared/net/annotation.h"

namespace net {

LoopbackServer::LoopbackServer(QObject *parent)
	: QObject(parent),
	  Server(true),
	  _layer_ids(255), _annotation_ids(255)
{
}
	
void LoopbackServer::reset()
{
	_layer_ids.reset();
	_annotation_ids.reset();
}

void LoopbackServer::logout()
{
	qWarning() << "tried to log out from the loopback server!";
}

void LoopbackServer::sendMessage(protocol::MessagePtr msg)
{
	// Keep track of layer and annotation IDs.
	switch(msg->type()) {
		using namespace protocol;
		case MSG_LAYER_CREATE: {
			LayerCreate &lc = msg.cast<LayerCreate>();
			if(lc.id() == 0)
				lc.setId(_layer_ids.takeNext());
			else
				_layer_ids.reserve(lc.id());
			break;
		}
		case MSG_ANNOTATION_CREATE: {
			AnnotationCreate &ac = msg.cast<AnnotationCreate>();
			if(ac.id() == 0)
				ac.setId(_annotation_ids.takeNext());
			else
				_annotation_ids.reserve(ac.id());
			break;
		}
		default: /* no special handling needed */ break;
	}
	emit messageReceived(msg);
}

void LoopbackServer::sendSnapshotMessages(QList<protocol::MessagePtr>)
{
	// There are no snapshots in loopback mode
}

}
