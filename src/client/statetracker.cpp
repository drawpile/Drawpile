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

#include "statetracker.h"

#include "core/layerstack.h"
#include "core/layer.h"
#include "../shared/net/pen.h"
#include "../shared/net/layer.h"
#include "../shared/net/image.h"

namespace drawingboard {

StateTracker::StateTracker(dpcore::LayerStack *image, QObject *parent)
	: QObject(parent), _image(image)
{
}
	
void StateTracker::receiveCommand(protocol::Message *msg)
{
	switch(msg->type()) {
		using namespace protocol;
		case MSG_CANVAS_RESIZE:
			handleCanvasResize(*static_cast<CanvasResize*>(msg));
			break;
		case MSG_LAYER_CREATE:
			handleLayerCreate(*static_cast<LayerCreate*>(msg));
			break;
		case MSG_LAYER_ATTR:
			handleLayerAttributes(*static_cast<LayerAttributes*>(msg));
			break;
		case MSG_LAYER_ORDER:
			handleLayerOrder(*static_cast<LayerOrder*>(msg));
			break;
		case MSG_LAYER_DELETE:
			handleLayerDelete(*static_cast<LayerDelete*>(msg));
			break;
		case MSG_TOOLCHANGE:
			handleToolChange(*static_cast<ToolChange*>(msg));
			break;
		case MSG_PEN_MOVE:
			handlePenMove(*static_cast<PenMove*>(msg));
			break;
		case MSG_PEN_UP:
			handlePenUp(*static_cast<PenUp*>(msg));
			break;
		case MSG_PUTIMAGE:
			handlePutImage(*static_cast<PutImage*>(msg));
			break;
		default:
			qDebug() << "Unhandled command" << msg->type();
	}
	
	// TODO
	delete msg;
}

void StateTracker::handleCanvasResize(const protocol::CanvasResize &cmd)
{
	// TODO support actual resizing
	_image->init(QSize(cmd.width(), cmd.height()));
}

void StateTracker::handleLayerCreate(const protocol::LayerCreate &cmd)
{
	_image->addLayer(cmd.id(), cmd.title(), cmd.fill());
}

void StateTracker::handleLayerAttributes(const protocol::LayerAttributes &cmd)
{
}

void StateTracker::handleLayerOrder(const protocol::LayerOrder &cmd)
{
}

void StateTracker::handleLayerDelete(const protocol::LayerDelete &cmd)
{
	_image->deleteLayer(cmd.id());
}

void StateTracker::handleToolChange(const protocol::ToolChange &cmd)
{
	DrawingContext &ctx = _contexts[cmd.contextId()];
	dpcore::Brush &b = ctx.tool.brush;
	ctx.tool.layer_id = cmd.layer();
	b.setBlendingMode(cmd.blend());
	b.setSubpixel(cmd.mode() & protocol::TOOL_MODE_SUBPIXEL);
	b.setSpacing(cmd.spacing());
	b.setRadius(cmd.size_h());
	b.setRadius2(cmd.size_l());
	b.setHardness(cmd.hard_h() / 255.0);
	b.setHardness2(cmd.hard_l() / 255.0);
	b.setOpacity(cmd.opacity_h() / 255.0);
	b.setOpacity2(cmd.opacity_l() / 255.0);
	b.setColor(cmd.color_h());
	b.setColor2(cmd.color_l());
}

void StateTracker::handlePenMove(const protocol::PenMove &cmd)
{
	DrawingContext &ctx = _contexts[cmd.contextId()];
	dpcore::Point p;
	foreach(const protocol::PenPoint pp, cmd.points()) {
		p = dpcore::Point(pp.x, pp.y, pp.p/255.0);
		
		dpcore::Layer *layer = _image->getLayer(ctx.tool.layer_id);
		
		if(ctx.pendown) {
			layer->drawLine(ctx.tool.brush, ctx.lastpoint, p, ctx.distance_accumulator);
		} else {
			ctx.pendown = true;
			ctx.distance_accumulator = 0;
			layer->dab(ctx.tool.brush, p);
		}
		ctx.lastpoint = p;
	}
}

void StateTracker::handlePenUp(const protocol::PenUp &cmd)
{
	DrawingContext &ctx = _contexts[cmd.contextId()];
	ctx.pendown = false;
}

void StateTracker::handlePutImage(const protocol::PutImage &cmd)
{
	QByteArray data = qUncompress(cmd.image());
	QImage img(reinterpret_cast<const uchar*>(data.constData()), cmd.width(), cmd.height(), QImage::Format_ARGB32);
	dpcore::Layer *layer = _image->getLayer(cmd.layer());
	layer->putImage(cmd.x(), cmd.y(), img, (cmd.flags() & protocol::PutImage::MODE_BLEND));
}

}
