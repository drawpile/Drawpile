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
#ifndef TOOLS_TOOL_H
#define TOOLS_TOOL_H

#include <QHash>
#include <QPointer>
#include <QAbstractGraphicsShapeItem>

#include "core/point.h"

namespace drawingboard {
    class CanvasScene;
}

namespace docks {
	class ToolSettings;
}

namespace net {
	class Client;
}

/**
 * @brief Tools
 *
 * Tools translate commands from the local user into messages that
 * can be sent over the network.
 * Read-only tools can access the canvas directly.
 */
namespace tools {

enum Type {SELECTION, PEN, BRUSH, ERASER, PICKER, LINE, RECTANGLE, ELLIPSE, ANNOTATION, LASERPOINTER};

class ToolCollection;

/**
 * @brief Base class for all tools
 * Tool classes interpret mouse/pen commands into editing actions.
 */
class Tool
{
public:
	Tool(ToolCollection &owner, Type type)
		: _owner(owner), _type(type)
		{}
	virtual ~Tool() = default;

	//! Get the type of this tool
	Type type() const { return _type; }

	/**
	 * @brief Start a new stroke
	 * @param point starting point
	 * @param right is the right mouse/pen button pressed instead of the left one
	 */
	virtual void begin(const paintcore::Point& point, bool right) = 0;

	/**
	 * @brief Continue a stroke
	 * @param point new point
	 * @param constrain is the "constrain motion" button pressed
	 * @param cener is the "center on start point" button pressed
	 */
	virtual void motion(const paintcore::Point& point, bool constrain, bool center) = 0;

	//! End drawing
	virtual void end() = 0;

	//! Does this tool allow stroke smoothing to be used?
	virtual bool allowSmoothing() const { return false; }

protected:
	inline docks::ToolSettings &settings();
	inline net::Client &client();
	inline drawingboard::CanvasScene &scene();
	inline int layer();

private:
	ToolCollection &_owner;
	const Type _type;
};

/**
 * @brief A collection for tool instances.
 *
 * Note. This is not a singleton. Each mainwindow instance gets its own tool collection.
 */
class ToolCollection {
	friend class Tool;
	public:
		ToolCollection();
		~ToolCollection();

		//! Set network client to use
		void setClient(net::Client *client);

        //! Set the canvas scene to use
        void setScene(drawingboard::CanvasScene *scene);

		//! Set the tool settings widget from which current settings are fetched
		void setToolSettings(docks::ToolSettings *settings);

		//! Get the tool settings widget
		docks::ToolSettings *toolsettings() const { return _toolsettings; }

		//! Set the currently active layer
		void selectLayer(int layer_id);

		//! Get an instance of a specific tool
		Tool *get(Type type);

	private:
		net::Client *_client;
        drawingboard::CanvasScene *_scene;
		docks::ToolSettings *_toolsettings;
		QHash<Type, Tool*> _tools;
		int _layer;

};

net::Client &Tool::client() { return *_owner._client; }
docks::ToolSettings &Tool::settings() { return *_owner._toolsettings; }
drawingboard::CanvasScene &Tool::scene() { return *_owner._scene; }
int Tool::layer() { return _owner._layer; }

}

#endif

