/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2009 Calle Laakkonen

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
#ifndef ORAWRITER_H
#define ORAWRITER_H

#include <QStringList>

namespace dpcore {
	class LayerStack;
}

class Zipfile;

namespace openraster {

//! OpenRaster writer
class Writer {
	public:
		Writer(const dpcore::LayerStack *layers);

		//! Set the annotations to save
		void setAnnotations(const QStringList& annotations);

		//! Save the layers as an OpenRaster image
		bool save(const QString& filename) const;

	private:
		bool writeStackXml(Zipfile &zf) const;
		bool writeLayer(Zipfile &zf, int index) const;
		bool writeThumbnail(Zipfile &zf) const;

		const dpcore::LayerStack *layers_;
		QStringList annotations_;
};

}

#endif

