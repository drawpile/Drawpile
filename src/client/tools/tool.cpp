/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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

#include "tools/annotation.h"
#include "tools/brushes.h"
#include "tools/colorpicker.h"
#include "tools/laser.h"
#include "tools/selection.h"
#include "tools/shapetools.h"

namespace tools {

/**
 * Construct one of each tool for the collection
 */
ToolCollection::ToolCollection()
	: _client(0), _toolsettings(0)
{
	_tools[PEN] = new Pen(*this);
	_tools[BRUSH] = new Brush(*this);
	_tools[ERASER] = new Eraser(*this);
	_tools[PICKER] = new ColorPicker(*this);
	_tools[LINE] = new Line(*this);
	_tools[RECTANGLE] = new Rectangle(*this);
	_tools[ELLIPSE] = new Ellipse(*this);
	_tools[ANNOTATION] = new Annotation(*this);
	_tools[SELECTION] = new Selection(*this);
	_tools[LASERPOINTER] = new LaserPointer(*this);
}

/**
 * Delete the tools
 */
ToolCollection::~ToolCollection()
{
	foreach(Tool *t, _tools)
		delete t;
}

void ToolCollection::setClient(net::Client *client)
{
	Q_ASSERT(client);
	_client = client;
}

void ToolCollection::setScene(drawingboard::CanvasScene *scene)
{
    Q_ASSERT(scene);
    _scene = scene;
}

void ToolCollection::setToolSettings(docks::ToolSettings *s)
{
	Q_ASSERT(s);
	_toolsettings = s;
}

void ToolCollection::selectLayer(int layer_id)
{
	_layer = layer_id;
}

/**
 * The returned tool can be used to perform actions on the board
 * controlled by the specified controller.
 * 
 * @param type type of tool wanted
 * @return the requested tool
 */
Tool *ToolCollection::get(Type type)
{
	Q_ASSERT(_tools.contains(type));
	return _tools.value(type);
}

}
