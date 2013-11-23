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

#include <QDebug>

#include "loopbackserver.h"

#ifdef LAG_SIMULATOR
#include <QTimer>
#endif

#include "../shared/net/layer.h"
#include "../shared/net/annotation.h"

namespace net {

LoopbackServer::LoopbackServer(QObject *parent)
	: QObject(parent),
#ifdef LAG_SIMULATOR
	  Server(false),
#else
	  Server(true),
  #endif
	  _layer_ids(255), _annotation_ids(255)
{
#ifdef LAG_SIMULATOR
	_lagtimer = new QTimer(this);
	connect(_lagtimer, SIGNAL(timeout()), this, SLOT(sendDelayedMessage()));
#endif
}
	
void LoopbackServer::reset()
{
	_layer_ids.reset();
	_annotation_ids.reset();
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
#ifdef LAG_SIMULATOR
	_msgqueue.append(msg);
	if(!_lagtimer->isActive()) {
		_lagtimer->start(qrand() % LAG_SIMULATOR);
	}
#else
	emit messageReceived(msg);
#endif
}

void LoopbackServer::sendSnapshotMessages(QList<protocol::MessagePtr> msgs)
{
	// There are no snapshots in loopback mode
}

#ifdef LAG_SIMULATOR
void LoopbackServer::sendDelayedMessage()
{
	if(!_msgqueue.isEmpty()) {
		emit messageReceived(_msgqueue.takeFirst());
		_lagtimer->start(qrand() % LAG_SIMULATOR);
	}
}
#endif

}
