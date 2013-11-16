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
#ifndef DP_STATETRACKER_H
#define DP_STATETRACKER_H

#include <QObject>
#include <QHash>

#include "core/brush.h"
#include "core/point.h"

namespace protocol {
	class Message;
	class CanvasResize;
	class LayerCreate;
	class LayerAttributes;
	class LayerOrder;
	class LayerDelete;
	class ToolChange;
	class PenMove;
	class PenUp;
	class PutImage;
}

namespace dpcore {
	class LayerStack;
}

namespace drawingboard {

struct ToolContext {
	int layer_id;
	dpcore::Brush brush;
};

struct DrawingContext {
	DrawingContext() : pendown(false), distance_accumulator(0) {}
	
	ToolContext tool;
	dpcore::Point lastpoint;
	bool pendown;
	qreal distance_accumulator;
};

class StateTracker : public QObject {
	Q_OBJECT
public:
	StateTracker(dpcore::LayerStack *_image, QObject *parent=0);
	
	void receiveCommand(protocol::Message *msg);
	
private:
	// Layer related commands
	void handleCanvasResize(const protocol::CanvasResize &cmd);
	void handleLayerCreate(const protocol::LayerCreate &cmd);
	void handleLayerAttributes(const protocol::LayerAttributes &cmd);
	void handleLayerOrder(const protocol::LayerOrder &cmd);
	void handleLayerDelete(const protocol::LayerDelete &cmd);
	
	// Drawing related commands
	void handleToolChange(const protocol::ToolChange &cmd);
	void handlePenMove(const protocol::PenMove &cmd);
	void handlePenUp(const protocol::PenUp &cmd);
	void handlePutImage(const protocol::PutImage &cmd);
	
	// Annotation related commands
	
	QHash<int, DrawingContext> _contexts;
	
	dpcore::LayerStack *_image;
};

}

#endif
