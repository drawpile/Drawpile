/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2009-2013 Calle Laakkonen

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

namespace widgets {

/**
 * A specialization of QMimeData for passing layers around inside
 * the application. Can also export the layer content as a regular
 * image on demand.
 */
class LayerMimeData : public QMimeData
{
	Q_OBJECT
	public:
		LayerMimeData(int layer_id);

#if 0
		//! Get the layer
		Layer *layer() const { return layer_; }
#endif
		int layerId() const { return _id; }

		//! Supported export formats
		QStringList formats() const;
	
	protected:
		QVariant retrieveData(const QString& mimeType, QVariant::Type type) const;

	private:
		int _id;
};

}

#endif

