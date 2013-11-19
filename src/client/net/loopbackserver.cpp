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
#include "loopbackserver.h"

#include "../shared/net/layer.h"
#include "../shared/net/annotation.h"

namespace net {

LoopbackServer::LoopbackServer(QObject *parent)
	: QObject(parent), _layer_ids(255), _annotation_ids(255)
{
}
	
void LoopbackServer::reset()
{
	_layer_ids.reset();
	_annotation_ids.reset();
}

void LoopbackServer::sendMessage(protocol::Message *msg)
{
	// Keep track of layer and annotation IDs.
	switch(msg->type()) {
		using namespace protocol;
		case MSG_LAYER_CREATE: {
			LayerCreate *lc = static_cast<LayerCreate*>(msg);
			if(lc->id() == 0)
				lc->setId(_layer_ids.takeNext());
			else
				_layer_ids.reserve(lc->id());
			break;
		}
		case MSG_ANNOTATION_CREATE: {
			AnnotationCreate *ac = static_cast<AnnotationCreate*>(msg);
			if(ac->id() == 0)
				ac->setId(_annotation_ids.takeNext());
			else
				_annotation_ids.reserve(ac->id());
			break;
		}
		default: /* no special handling needed */ break;
	}
	emit messageReceived(msg);
}

}
