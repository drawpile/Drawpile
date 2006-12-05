/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

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

namespace drawingboard {
	class BoardEditor;
}

namespace tools {

enum Type {BRUSH, ERASER, PICKER};

//! Base class for all tools
/**
 * Tool classes interpret mouse/pen commands into editing actions.
 * The BoardEditor is used to actually commit those actions.
 */
class Tool
{
	public:
		Tool(Type type)
			: type_(type) {}
		virtual ~Tool() {};

		//! Get an instance of a specific tool
		static Tool *get(drawingboard::BoardEditor *editor, Type type);

		//! Get the type of this tool
		Type type() const { return type_; }

		//! Begin drawing
		virtual void begin(int x,int y, qreal pressure) = 0;

		//! Draw stroke
		virtual void motion(int x,int y, qreal pressure) = 0;

		//! End drawing
		virtual void end() = 0;

	protected:
		static drawingboard::BoardEditor *editor_;

	private:
		Type type_;

};

//! Base class for brush type tools
/**
 * Brush type tools change to drawing board.
 */
class BrushBase : public Tool
{
	public:
		BrushBase(Type type) : Tool(type) {}

		void begin(int x,int y, qreal perssure);
		void motion(int x,int y, qreal pressure);
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
		ColorPicker() : Tool(PICKER) {}

		void begin(int x,int y, qreal perssure);
		void motion(int x,int y, qreal pressure);
		void end();
};

}

#endif

