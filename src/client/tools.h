/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2007 Calle Laakkonen

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

#include "point.h"

namespace drawingboard {
	class BoardEditor;
}

//! Tools
/**
 * Tools translate commands from the local user into messages that
 * can be sent over the network or directly modify the drawingboard
 * if in offline mode. Some tools do not modify the drawing board
 * and always access the it directly.
 *
 * The BoardEditor class is used to abstract away the difference between
 * local and remote drawing boards.
 */
namespace tools {

enum Type {BRUSH, ERASER, PICKER, LINE, RECTANGLE};

//! Base class for all tools
/**
 * Tool classes interpret mouse/pen commands into editing actions.
 * The BoardEditor is used to actually commit those actions.
 */
class Tool
{
	public:
		Tool(Type type, bool readonly)
			: type_(type), readonly_(readonly) {}
		virtual ~Tool() {};

		//! Set board editor to use
		static void setEditor(drawingboard::BoardEditor *editor);

		//! Get an instance of a specific tool
		static Tool *get(Type type);

		//! Get the type of this tool
		Type type() const { return type_; }

		//! Is this a read only tool (ie. color picker)
		bool readonly() const { return readonly_; }

		//! Begin drawing
		virtual void begin(const drawingboard::Point& point) = 0;

		//! Draw stroke
		virtual void motion(const drawingboard::Point& point) = 0;

		//! End drawing
		virtual void end() = 0;

	protected:
		static drawingboard::BoardEditor *editor_;

	private:
		const Type type_;
		const bool readonly_;

};

//! Base class for brush type tools
/**
 * Brush type tools change to drawing board.
 */
class BrushBase : public Tool
{
	public:
		BrushBase(Type type) : Tool(type, false) {}

		void begin(const drawingboard::Point& point);
		void motion(const drawingboard::Point& point);
		void end();
};

//! Regular brush
class Brush : public BrushBase {
	public:
		Brush() : BrushBase(BRUSH) {}
};

//! Eraser
class Eraser : public BrushBase {
	public:
		Eraser() : BrushBase(ERASER) {}
};

//! Color picker
/**
 * Color picker is a local tool, it does not affect the drawing board.
 */
class ColorPicker : public Tool {
	public:
		ColorPicker() : Tool(PICKER, true) {}

		void begin(const drawingboard::Point& point);
		void motion(const drawingboard::Point& point);
		void end();
};

//! Base class for complex tools
/**
 * This is a base class for tools that have special previewing
 * needs.
 */
class ComplexBase : public Tool {
	public:
		ComplexBase(Type type) : Tool(type, false) {}

		void begin(const drawingboard::Point& point);
		void motion(const drawingboard::Point& point);
		void end();

	protected:
		virtual void commit() = 0;

		drawingboard::Point start_;
		drawingboard::Point end_;
};

//! Line tool
class Line : public ComplexBase {
	public:
		Line() : ComplexBase(LINE) {}

	protected:
		void commit();

};

//! Rectangle tool
class Rectangle : public ComplexBase {
	public:
		Rectangle() : ComplexBase(RECTANGLE) {}

	protected:
		void commit();
};

}

#endif

