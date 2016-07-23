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

#include <QDebug>
#include <QDataStream>
#include <QFile>
#include <QCryptographicHash>

#include "index.h"

namespace recording {

int Index::findPreviousStop(unsigned int pos) const
{
	int i=0;
	while(i<m_stops.size() && m_stops.at(i).index < pos)
		++i;
	return qMax(0, i-1);
}

int Index::findClosestSnapshot(unsigned int pos) const
{
	int i=0;
	int snap=0;
	while(i<m_stops.size() && m_stops.at(i).index < pos) {
		if((m_stops.at(i).flags & StopEntry::HAS_SNAPSHOT))
			snap = i;
		++i;
	}
	return snap;
}

bool Index::writeIndex(QIODevice *out) const
{
	QDataStream ds(out);
	ds.setVersion(QDataStream::Qt_5_6);

	// Write index format version
	ds << INDEX_VERSION;

	// Write stops
	ds << quint32(m_stops.size());
	for(const StopEntry &e : m_stops) {
		ds << e.index << e.pos << e.flags;
	}

	// Write markers
	ds << quint32(m_markers.size());
	for(const MarkerEntry &e : m_markers) {
		ds << e.stop << e.title;
	}

	// Write action count
	ds << quint32(m_actioncount);

	return true;
}

bool Index::readIndex(QIODevice *out)
{
	QDataStream ds(out);
	ds.setVersion(QDataStream::Qt_5_6);

	// Read version
	quint16 version;
	ds >> version;
	if(version != INDEX_VERSION) {
		qWarning() << "Wrong index version:" << version;
		return false;
	}

	// Read stops
	quint32 stopcount;
	QVector<StopEntry> stops;
	QList<int> thumbs;
	ds >> stopcount;
	while(stopcount--) {
		StopEntry e;
		ds >> e.index >> e.pos >> e.flags;
		stops.append(e);
	}

	// Read markers
	quint32 markercount;
	QVector<MarkerEntry> markers;
	ds >> markercount;
	while(markercount--) {
		MarkerEntry e;
		ds >> e.stop >> e.title;
		markers.append(e);
	}

	// Read action count
	quint32 actioncount;
	ds >> actioncount;

	m_stops = stops;
	m_markers = markers;
	m_actioncount = actioncount;

	return true;
}

QByteArray hashRecording(const QString &filename)
{
	QFile file(filename);
	if(!file.open(QFile::ReadOnly))
		return QByteArray();

	QCryptographicHash hash(QCryptographicHash::Sha1);
	hash.addData(&file);

	return hash.result();
}

}

