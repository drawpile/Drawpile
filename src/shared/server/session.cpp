/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2013 Calle Laakkonen

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

#include "session.h"
#include "../net/annotation.h"
#include "../net/layer.h"
#include "../net/pen.h"

namespace server {

void SessionState::syncInitialState(const QList<protocol::MessagePtr> &messages)
{
	using namespace protocol;
	foreach(MessagePtr msg, messages) {
		switch(msg->type()) {
		case MSG_LAYER_CREATE:
			createLayer(msg.cast<LayerCreate>(), false);
			break;
		case MSG_LAYER_ORDER:
			reorderLayers(msg.cast<LayerOrder>());
			break;
		case MSG_LAYER_DELETE:
			deleteLayer(msg.cast<LayerDelete>().id());
			break;
		case MSG_ANNOTATION_CREATE:
			createAnnotation(msg.cast<AnnotationCreate>(), false);
			break;
		case MSG_ANNOTATION_DELETE:
			deleteAnnotation(msg.cast<AnnotationDelete>().id());
			break;
		case MSG_TOOLCHANGE:
			userids.reserve(msg.cast<ToolChange>().contextId());
			break;
		default: break;
		}
	}
}

void SessionState::createLayer(protocol::LayerCreate &cmd, bool assign)
{
	if(assign)
		cmd.setId(layerids.takeNext());
	else
		layerids.reserve(cmd.id());
	layers.append(LayerState(cmd.id()));
}

void SessionState::reorderLayers(protocol::LayerOrder &cmd)
{
	QVector<LayerState> newlayers;
	QVector<LayerState> oldlayers = layers;
	newlayers.reserve(layers.size());

	// Set new order
	foreach(int id, cmd.order()) {
		for(int i=0;i<oldlayers.size();++i) {
			if(oldlayers[i].id == id) {
				newlayers.append(oldlayers[i]);
				oldlayers.remove(i);
			}
		}
	}
	// If there were any layers that were missed, add them to
	// the top of the stack in their original relative order
	for(int i=0;i<oldlayers.size();++i)
		newlayers.append(oldlayers[i]);

	layers = newlayers;

	// Update commands ID list
	QList<uint8_t> validorder;
	validorder.reserve(layers.size());
	for(int i=0;i<layers.size();++i)
		validorder.append(layers[i].id);

	cmd.setOrder(validorder);
}

bool SessionState::deleteLayer(int id)
{
	for(int i=0;i<layers.size();++i) {
		if(layers[i].id == id) {
			layers.remove(i);
			layerids.release(id);
			return true;
		}
	}
	return false;
}

void SessionState::createAnnotation(protocol::AnnotationCreate &cmd, bool assign)
{
	if(assign)
		cmd.setId(annotationids.takeNext());
	else
		annotationids.reserve(cmd.id());
}

bool SessionState::deleteAnnotation(int id)
{
	annotationids.release(id);
	return true; // TODO implement ID tracker properly
}

}
