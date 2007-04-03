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
#ifndef LOCALPALETTE_H
#define LOCALPALETTE_H

#include <QList>
#include <QVariant>

#include "palette.h"

class LocalPalette : public Palette {
	public:
		//! Construct a blank palette
		LocalPalette(const QString& name) : name_(name) {}
		//! Construct a palette from a list of QVariants that contain QColors
		LocalPalette(const QString& name, const QList<QVariant>& list);

		//! Generate a default palette
		static LocalPalette *makeDefaultPalette();

		//! Set the name of the palette
		void setName(const QString& name) { name_ = name; }

		//! Get the name of the palette
		const QString& name() const { return name_; }

		int count() const;
		QColor color(int index) const;
		void setColor(int index, const QColor& color);
		void insertColor(int index, const QColor& color);
		void removeColor(int index);

		//! Get the palette contents as a list of QVariants
		QList<QVariant> toVariantList() const;

	private:
		QList<QColor> colors_;
		QString name_;
};

#endif

