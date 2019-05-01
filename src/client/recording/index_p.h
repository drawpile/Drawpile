/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef REC_INDEX_PRIVATE_H
#define REC_INDEX_PRIVATE_H

#include "../core/layer.h"

#include <QVector>
#include <QString>
#include <QDataStream>

namespace recording {

//! Index format version
static const quint32 INDEX_VERSION = 7;

struct IndexedLayer {
	QVector<quint32> tileOffsets;
	QVector<quint32> sublayerOffsets;
	paintcore::LayerInfo info;
};

QDataStream &operator>>(QDataStream&, IndexedLayer&);
QDataStream &operator<<(QDataStream&, const IndexedLayer&);

struct IndexedLayerStack {
	QVector<quint32> layerOffsets;
	QVector<quint32> annotationOffsets;
	quint32 backgroundTileOffset;
	QSize size;
};

QDataStream &operator>>(QDataStream&, IndexedLayerStack&);
QDataStream &operator<<(QDataStream&, const IndexedLayerStack&);

}

#endif
