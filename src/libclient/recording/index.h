/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

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
#ifndef REC_INDEX_H
#define REC_INDEX_H

#include <QVector>
#include <QString>
#include <QImage>

namespace recording {

struct IndexEntry {
	//! Number of the message in the recording file
	quint32 index;

	//! Message position in the recording file
	qint64 messageOffset;

	//! Offset of the canvas snapshot in the index file
	qint64 snapshotOffset;

	//! Title of this entry (if this is a marker)
	QString title;

	//! The thumbnail of this entry
	QImage thumbnail;

	/**
	 * Find the index entry closest to the given position, such that entry.index <= pos.
	 *
	 * @return stop index or 0 if not found
	 */
	static IndexEntry nearest(const QVector<IndexEntry> &index, int pos);
};

QDataStream &operator>>(QDataStream&, IndexEntry&);
QDataStream &operator<<(QDataStream&, const IndexEntry&);

//! Hash the recording file
QByteArray hashRecording(const QString &filename);

}

#endif

