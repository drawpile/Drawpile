/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2008 Calle Laakkonen

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

#include "core/point.h"

namespace drawingboard {
	class AnnotationItem;
}

namespace widgets {
	class ToolSettings;
}

namespace net {
	class Client;
}

//! Tools
/**
 * Tools translate commands from the local user into messages that
 * can be sent over the network or directly modify the drawingboard
 * if in offline mode. Read-only tools access the canvas directly.
 */
namespace tools {

enum Type {PEN, BRUSH, ERASER, PICKER, LINE, RECTANGLE, ANNOTATION};

class ToolCollection;

//! Base class for all tools
/**
 * Tool classes interpret mouse/pen commands into editing actions.
 * The BoardEditor is used to actually commit those actions.
 */
class Tool
{
	public:
		Tool(ToolCollection &owner, Type type, bool readonly)
			: _owner(owner), _type(type), _readonly(readonly)
			{}
		virtual ~Tool() = default;

		//! Get the type of this tool
		Type type() const { return _type; }

		//! Is this a read only tool (ie. color picker)
		bool readonly() const { return _readonly; }

		//! Begin drawing
		virtual void begin(const dpcore::Point& point) = 0;

		//! Draw stroke
		virtual void motion(const dpcore::Point& point) = 0;

		//! End drawing
		virtual void end() = 0;

	protected:
		inline widgets::ToolSettings &settings();
		inline net::Client &client();
		inline int layer();

	private:
		ToolCollection &_owner;
		const Type _type;
		const bool _readonly;

};

//! Base class for brush type tools
/**
 * Brush type tools change to drawing board.
 */
class BrushBase : public Tool
{
	public:
		BrushBase(ToolCollection &owner, Type type) : Tool(owner, type, false) {}

		void begin(const dpcore::Point& point);
		void motion(const dpcore::Point& point);
		void end();
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

//! Color picker
/**
 * Color picker is a local tool, it does not affect the drawing board.
 */
class ColorPicker : public Tool {
	public:
		ColorPicker(ToolCollection &owner) : Tool(owner, PICKER, true) {}

		void begin(const dpcore::Point& point);
		void motion(const dpcore::Point& point);
		void end();
};

//! Base class for complex tools
/**
 * This is a base class for tools that have special previewing
 * needs.
 */
class ComplexBase : public Tool {
	public:
		ComplexBase(ToolCollection &owner, Type type) : Tool(owner, type, false) {}

		void begin(const dpcore::Point& point);
		void motion(const dpcore::Point& point);
		void end();

	protected:
		virtual void commit() = 0;

		dpcore::Point start_;
		dpcore::Point end_;
};

//! Line tool
class Line : public ComplexBase {
	public:
		Line(ToolCollection &owner) : ComplexBase(owner, LINE) {}

	protected:
		void commit();

};

//! Rectangle tool
class Rectangle : public ComplexBase {
	public:
		Rectangle(ToolCollection &owner) : ComplexBase(owner, RECTANGLE) {}

	protected:
		void commit();
};

//! Annotation tool
/**
 * This is a remote tool, but it only affects annotations that are
 * separate from the pixel data.
 */
class Annotation : public Tool {
	public:
		Annotation(ToolCollection &owner) : Tool(owner, ANNOTATION, true), sel_(0) { }

		void begin(const dpcore::Point& point);
		void motion(const dpcore::Point& point);
		void end();

	private:
		drawingboard::AnnotationItem *sel_;
		int handle_;
		QPoint start_, end_;
};


/**
 * A collection for tools, specific to a single controller.
 */
class ToolCollection {
	friend class Tool;
	public:
		ToolCollection();
		~ToolCollection();

		//! Set network client to use
		void setClient(net::Client *client);

		//! Set the tool settings widget from which current settings are fetched
		void setToolSettings(widgets::ToolSettings *as);

		//! Set the currently active layer
		void selectLayer(int layer_id);

		//! Get an instance of a specific tool
		Tool *get(Type type);

	private:
		net::Client *_client;
		widgets::ToolSettings *_toolsettings;
		QHash<Type, Tool*> _tools;
		int _layer;

};

net::Client &Tool::client() { return *_owner._client; }
widgets::ToolSettings &Tool::settings() { return *_owner._toolsettings; }
int Tool::layer() { return _owner._layer; }

}

#endif

