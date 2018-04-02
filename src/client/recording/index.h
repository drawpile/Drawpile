/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2016 Calle Laakkonen

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

class QIODevice;

namespace recording {

//! Index format version
static const quint16 INDEX_VERSION = 0x0005;

struct StopEntry {
	static const quint8 HAS_SNAPSHOT = 0x01;

	//! Index number of the message
	quint32 index;

	//! Message position in the recording file
	qint64 pos;

	//! Extra info about this index position
	quint8 flags;
};

struct MarkerEntry {
	quint32 stop;
	QString title;
};

/**
 * Recording index
 */
class Index {
	friend class IndexBuilder;
	friend class IndexLoader;

public:

	//! Get the number of entries in the index
	int size() const { return m_stops.size(); }

	//! Get the given stop
	const StopEntry &entry(int stopIdx) const { return m_stops.at(stopIdx); }

	//! Get the stop vector
	const QVector<StopEntry> &entries() const { return m_stops; }

	//! Get the marker vector
	const QVector<MarkerEntry> &markers() const { return m_markers; }

	//! Get the total number of actions in the recording
	int actionCount() const { return m_actioncount; }

	/**
	 * Find the stop closest to the given position, such that stop.index < pos.
	 * Returns 0 if no such stop is found.
	 * @return stop index
	 */
	int findPreviousStop(unsigned int pos) const;

	/**
	 * Find the snapshot S nearest to the given index I, such that S < I.
	 */
	int findClosestSnapshot(unsigned int pos) const;

	bool writeIndex(QIODevice *out) const;
	bool readIndex(QIODevice *in);

private:
	QVector<StopEntry> m_stops;
	QVector<MarkerEntry> m_markers;
	int m_actioncount;
};

//! Hash the recording file
QByteArray hashRecording(const QString &filename);

}

#endif
