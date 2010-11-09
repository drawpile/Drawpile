/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2009-2010 Calle Laakkonen

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
#ifndef ORAREADER_H
#define ORAREADER_H

#include <QRect>

namespace dpcore {
	class LayerStack;
}

class Zipfile;
class QDomElement;
class QPoint;

namespace openraster {

/**
 * An OpenRaster reader.
 */
class Reader {
	public:
		enum Warning {
			//! No warnings (i.e. DrawPile supports all features of the file)
			NO_WARNINGS = 0,
			//! The OpenRaster file uses unsupported app. specific extensions
			ORA_EXTENDED = 0x01,
			//! Nested layers are used
			ORA_NESTED = 0x02,
		};
		Q_DECLARE_FLAGS(Warnings, Warning)

		Reader(const QString& file);
		~Reader();

		//! Load the image
		bool load();

		//! Get loaded annotations
		const QStringList& annotations() const { return annotations_; }

		//! Get the loaded layers
		dpcore::LayerStack *layers() const { return stack_; }

		//! Get the error message
		const QString& error() const { return error_; }

		//! Get the warning flags
		const Warnings warnings() const { return warnings_; }
	private:
		bool loadLayers(const QDomElement& stack, QPoint offset);
		void loadAnnotations(const QDomElement& annotations);

		Zipfile *ora_;
		QStringList annotations_;
		dpcore::LayerStack *stack_;
		QString error_;
		Warnings warnings_;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Reader::Warnings)

}

#endif

