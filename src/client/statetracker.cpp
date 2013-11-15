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

#include "canvasitem.h"
#include "statetracker.h"

#include "../shared/net/pen.h"

StateTracker::StateTracker(drawingboard::CanvasItem *image, QObject *parent)
	: QObject(parent), _image(image)
{
}
	
void StateTracker::receiveCommand(protocol::Message *msg)
{
	switch(msg->type()) {
		using namespace protocol;
		case MSG_TOOLCHANGE:
			handleToolChange(*static_cast<ToolChange*>(msg));
			break;
		case MSG_PEN_MOVE:
			handlePenMove(*static_cast<PenMove*>(msg));
			break;
		case MSG_PEN_UP:
			handlePenUp(*static_cast<PenUp*>(msg));
			break;
		default:
			qDebug() << "Unhandled command" << msg->type();
	}
	
	// TODO
	delete msg;
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
		
		if(ctx.pendown) {
			_image->drawLine(ctx.tool.layer_id, ctx.lastpoint, p, ctx.tool.brush, ctx.distance_accumulator);
		} else {
			ctx.pendown = true;
			ctx.distance_accumulator = 0;
			_image->drawPoint(ctx.tool.layer_id, p, ctx.tool.brush);
		}
		ctx.lastpoint = p;
	}
}

void StateTracker::handlePenUp(const protocol::PenUp &cmd)
{
	DrawingContext &ctx = _contexts[cmd.contextId()];
	ctx.pendown = false;
}
