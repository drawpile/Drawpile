/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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

#include "docks/toolsettingsdock.h"
#include "scene/canvasscene.h"
#include "core/brush.h"
#include "net/client.h"
#include "statetracker.h"


#include "tools/toolsettings.h"
#include "tools/laser.h"

namespace tools {

void LaserPointer::begin(const paintcore::Point &point, bool right)
{
	Q_UNUSED(right);
	// Send initial point to serve as the start of the line,
	// and also a toolchange to set the laser color

	const paintcore::Brush &brush = settings().getBrush(right);
	drawingboard::ToolContext tctx = {
		layer(),
		brush
	};
	client().sendToolChange(tctx);
	client().sendLaserPointer(point.x(), point.y(), 0);
}

void LaserPointer::motion(const paintcore::Point &point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);
	client().sendLaserPointer(point.x(), point.y(), settings().getLaserPointerSettings()->trailPersistence());
}

void LaserPointer::end()
{
	// Nothing to do here, laser trail does not need penup type notification
}

}

