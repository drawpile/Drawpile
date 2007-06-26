/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

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
#ifndef PALETTE_H
#define PALETTE_H

class QColor;

/**
 * \brief Color palette interface
 * This class provides an interface for reading and writing color palettes.
 */
class Palette {
	public:
		virtual ~Palette() {};

		//! Get the number of colors
		/**
		 * @return number of colors in the palette
		 */
		virtual int count() const = 0;

		//! Get the color at index
		/**
		 * @param index color index
		 * @return color at specified index
		 * @pre 0 <= index < count()
		 */
		virtual QColor color(int index) const = 0;

		//! Set a color
		/**
		 * @param index color index
		 * @param color color
		 * @pre 0 <= index < count()
		 */
		virtual void setColor(int index, const QColor& color) = 0;

		//! Insert a new color
		/**
		 * Size of the palette is increased by one. The new color is
		 * inserted before the index. If index == count(), the color is
		 * added to the end of the palette.
		 * @param index color index
		 * @param color color
		 * @pre 0 <= index <= count()
		 */
		virtual void insertColor(int index, const QColor& color) = 0;

		//! Remove a color
		/**
		 * Size of the palette is decreased by one.
		 * @param index color index
		 * @pre 0 <= index < count()
		 */
		virtual void removeColor(int index) = 0;
};

#endif

