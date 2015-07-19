/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

QString Index::contextName(int context_id) const
{
	if(m_ctxnames.contains(context_id))
		return m_ctxnames[context_id];
	return QString("#%1").arg(context_id);
}

bool Index::writeIndex(QIODevice *out) const
{
	QDataStream ds(out);
	ds.setVersion(QDataStream::Qt_5_0);

	// Write index format version
	ds << INDEX_VERSION;

	// Write context name map
	ds << quint8(m_ctxnames.size());
	foreach(quint8 ctx, m_ctxnames.keys()) {
		ds << ctx << m_ctxnames[ctx];
	}

	// Write index
	ds << quint32(m_index.size());
	foreach(const IndexEntry &e, m_index) {
		ds << quint8(e.type) << e.context_id << e.offset << e.start << e.end << e.color << e.title;
	}

	// Write snapshot list
	ds << quint32(m_snapshots.size());
	foreach(const SnapshotEntry &e, m_snapshots) {
		ds << e.stream_offset << e.pos;
	}

	return true;
}

bool Index::readIndex(QIODevice *out)
{
	QDataStream ds(out);
	ds.setVersion(QDataStream::Qt_5_0);

	// Read version
	quint16 version;
	ds >> version;
	if(version != INDEX_VERSION) {
		qWarning() << "Wrong index version:" << version;
		return false;
	}

	// Read context name map
	quint8 names;
	ds >> names;
	while(names-->0) {
		quint8 ctx;
		QString name;
		ds >> ctx >> name;
		m_ctxnames[ctx] = name;
	}

	// Read index
	quint32 entries;
	ds >> entries;
	while(entries--) {
		IndexEntry e;
		quint8 type;
		ds >> type;
		e.type = IndexType(type);
		ds >> e.context_id >> e.offset >> e.start >> e.end >> e.color >> e.title;
		m_index.append(e);
		if(e.type == IDX_MARKER)
			m_markers.append(MarkerEntry(m_index.size()-1, e.start, e.title));
	}

	// Read snapshot list
	quint32 snapshots;
	ds >> snapshots;
	while(snapshots--) {
		SnapshotEntry e;
		ds >> e.stream_offset >> e.pos;
		m_snapshots.append(e);
	}

	return true;
}

MarkerEntry Index::prevMarker(unsigned int from) const
{
	MarkerEntry e;
	for(int i=0;i<m_markers.size();++i) {
		if(m_markers[i].pos >= from)
			break;

		e = m_markers[i];
	}

	if(e.pos == from)
		e = MarkerEntry();

	return e;
}

MarkerEntry Index::nextMarker(unsigned int from) const
{
	MarkerEntry e;
	for(int i=m_markers.size()-1;i>=0;--i) {
		if(m_markers[i].pos <= from)
			break;

		e = m_markers[i];
	}

	if(e.pos == from)
		e = MarkerEntry();

	return e;
}

void Index::addMarker(qint64 offset, quint32 pos, const QString &title)
{
	IndexEntry e(IDX_MARKER, 0, offset, pos, pos, 0xffffffff, title);
	e.flags = IndexEntry::FLAG_ADDED;

	m_newmarkers.append(e);

	// Add entry to marker list
	int i=0;
	while(i<m_markers.size()) {
		if(m_markers.at(i).pos >= pos)
			break;
		++i;
	}
	m_markers.insert(i, MarkerEntry(0, pos, title));

}

void Index::setSilenced(int idx, bool silence)
{
	if(silence)
		m_silenced.insert(idx);
	else
		m_silenced.remove(idx);
}

IndexVector Index::silencedEntries() const
{
	IndexVector iv;
	for(int idx : silencedIndices()) {
		if(idx<0 || idx >= m_index.size())
			qWarning("Silenced non-existent index: %d", idx);
		else
			iv << m_index.at(idx);
	}
	return iv;
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

