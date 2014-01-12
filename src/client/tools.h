/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2013 Calle Laakkonen

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
#ifndef TOOLS_H
#define TOOLS_H

#include <QHash>
#include <QPointer>

#include "core/point.h"
#include "annotationitem.h"
#include "selectionitem.h"

namespace drawingboard {
	class AnnotationItem;
    class CanvasScene;
}

namespace widgets {
	class ToolSettingsDock;
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

enum Type {SELECTION, PEN, BRUSH, ERASER, PICKER, LINE, RECTANGLE, ANNOTATION};

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

	//! Begin drawing
	virtual void begin(const paintcore::Point& point, bool right) = 0;

	//! Draw stroke
	virtual void motion(const paintcore::Point& point) = 0;

	//! End drawing
	virtual void end() = 0;

	//! Does this tool allow stroke smoothing to be used?
	virtual bool allowSmoothing() const { return false; }

protected:
	inline widgets::ToolSettingsDock &settings();
	inline net::Client &client();
	inline drawingboard::CanvasScene &scene();
	inline int layer();

private:
	ToolCollection &_owner;
	const Type _type;
};

//! Base class for brush type tools
class BrushBase : public Tool
{
	public:
		BrushBase(ToolCollection &owner, Type type) : Tool(owner, type) {}

		void begin(const paintcore::Point& point, bool right);
		void motion(const paintcore::Point& point);
		void end();

		bool allowSmoothing() const { return true; }
};

//! Pen
class Pen : public BrushBase {
	public:
		Pen(ToolCollection &owner) : BrushBase(owner, PEN) {}
};

//! Regular brush
class Brush : public BrushBase {
	public:
		Brush(ToolCollection &owner) : BrushBase(owner, BRUSH) {}
};

//! Eraser
class Eraser : public BrushBase {
	public:
		Eraser(ToolCollection &owner) : BrushBase(owner, ERASER) {}
};

/**
 * @brief Color picker
 * Color picker is a local tool, it does not affect the drawing board.
 */
class ColorPicker : public Tool {
	public:
		ColorPicker(ToolCollection &owner) : Tool(owner, PICKER) {}

		void begin(const paintcore::Point& point, bool right);
		void motion(const paintcore::Point& point);
		void end();
};

//! Line tool
class Line : public Tool {
public:
	Line(ToolCollection &owner) : Tool(owner, LINE) {}

	void begin(const paintcore::Point& point, bool right);
	void motion(const paintcore::Point& point);
	void end();

private:
	paintcore::Point _p1, _p2;
	bool _swap;
};

//! Rectangle tool
class Rectangle : public Tool {
public:
	Rectangle(ToolCollection &owner) : Tool(owner, RECTANGLE) {}

	void begin(const paintcore::Point& point, bool right);
	void motion(const paintcore::Point& point);
	void end();

private:
	paintcore::Point _p1, _p2;
	bool _swap;
};

/**
 * @brief Annotation tool
 */
class Annotation : public Tool {
public:
	Annotation(ToolCollection &owner) : Tool(owner, ANNOTATION), _selected(0) { }

	void begin(const paintcore::Point& point, bool right);
	void motion(const paintcore::Point& point);
	void end();

private:
	QPointer<drawingboard::AnnotationItem> _selected;
	drawingboard::AnnotationItem::Handle _handle;
	QPointF _start, _end;
	bool _wasselected;
};

/**
 * @brief Selection tool
 *
 * This is used for selecting regions for copying & pasting.
 */
class Selection : public Tool {
public:
	Selection(ToolCollection &owner) : Tool(owner, SELECTION) {}

	void begin(const paintcore::Point& point, bool right);
	void motion(const paintcore::Point& point);
	void end();

	void clearSelection();

private:
	QPoint _start;
	drawingboard::SelectionItem::Handle _handle;
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
		void setToolSettings(widgets::ToolSettingsDock *settings);

		//! Set the currently active layer
		void selectLayer(int layer_id);

		//! Get an instance of a specific tool
		Tool *get(Type type);

	private:
		net::Client *_client;
        drawingboard::CanvasScene *_scene;
		widgets::ToolSettingsDock *_toolsettings;
		QHash<Type, Tool*> _tools;
		int _layer;

};

net::Client &Tool::client() { return *_owner._client; }
widgets::ToolSettingsDock &Tool::settings() { return *_owner._toolsettings; }
drawingboard::CanvasScene &Tool::scene() { return *_owner._scene; }
int Tool::layer() { return _owner._layer; }

}

#endif

