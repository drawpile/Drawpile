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
#ifndef LAYERMIMEDATA_H
#define LAYERMIMEDATA_H

#include <QMimeData>

namespace dpcore {

class Layer;

/**
 * A specialization of QMimeData for passing layers around inside
 * the application. Can also export the layer content as a regular
 * image on demand.
 */
class LayerMimeData : public QMimeData
{
	Q_OBJECT
	public:
		LayerMimeData(Layer *layer);

		//! Get the layer
		Layer *layer() const { return layer_; }

		//! Supported export formats
		QStringList formats() const;
	
	protected:
		QVariant retrieveData(const QString& mimeType, QVariant::Type type) const;

	private:
		Layer *layer_;
};

}

#endif

