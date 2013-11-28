/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2013 Calle Laakkonen

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
#ifndef LOCALPALETTE_H
#define LOCALPALETTE_H

#include <QList>

class QVariant;
class QFileInfo;

#include "palette.h"

class Palette {
	public:
		//! Construct a blank palette
		Palette(const QString& name, const QString& filename="");

		//! Load a palette from a file
		static Palette *fromFile(const QFileInfo& file);

		//! Generate a default palette
		static Palette *makeDefaultPalette();

		//! Set the name of the palette
		void setName(const QString& name);

		//! Get the name of the palette
		const QString& name() const { return name_; }

		//! Get the filename of the palette
		const QString& filename() const { return filename_; }

		//! Has the palette been modified since it was last saved/loaded?
		bool isModified() const { return modified_; }

		//! Save palette to file
		bool save(const QString& filename);

		//! Get the number of colors
		int count() const;
		
		//! Get the color at index
		QColor color(int index) const;
		
		//! Set a color at the specified index
		void setColor(int index, const QColor& color);
		
		//! Insert a new color
		void insertColor(int index, const QColor& color);

		//! Remove a color
		void removeColor(int index);

	private:
		QList<QColor> colors_;
		QString name_;
		QString filename_;
		bool modified_;
};

#endif

