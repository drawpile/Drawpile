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
#include <QStringList>

#include "layermimedata.h"

namespace widgets {

LayerMimeData::LayerMimeData(int layer_id)
	: QMimeData(), _id(layer_id)
{
}

QStringList LayerMimeData::formats() const
{
	return QStringList() << "image/png";
}

QVariant LayerMimeData::retrieveData(const QString& mimeType, QVariant::Type type) const
{
	// TODO
#if 0
	if(type!=QVariant::ByteArray || mimeType != "image/png")
		return QVariant();
	QByteArray img;
	QBuffer buf(&img);
	layer_->toImage().save(&buf, "PNG");
	return img;
#else
	return QVariant();
#endif
}

}

