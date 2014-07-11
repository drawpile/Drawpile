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
	if(_ctxnames.contains(context_id))
		return _ctxnames[context_id];
	return QString("#%1").arg(context_id);
}

bool Index::writeIndex(QIODevice *out) const
{
	QDataStream ds(out);
	ds.setVersion(QDataStream::Qt_5_0);

	// Write index format version
	ds << INDEX_VERSION;

	// Write context name map
	ds << quint8(_ctxnames.size());
	foreach(quint8 ctx, _ctxnames.keys()) {
		ds << ctx << _ctxnames[ctx];
	}

	// Write index
	ds << quint32(_index.size());
	foreach(const IndexEntry &e, _index) {
		ds << quint8(e.type) << e.context_id << e.offset << e.start << e.end << e.color << e.title;
	}

	// Write snapshot list
	ds << quint32(_snapshots.size());
	foreach(const SnapshotEntry &e, _snapshots) {
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
		_ctxnames[ctx] = name;
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
		_index.append(e);
		if(e.type == IDX_MARKER)
			_markers.append(MarkerEntry(_index.size()-1, e.start, e.title));
	}

	// Read snapshot list
	quint32 snapshots;
	ds >> snapshots;
	while(snapshots--) {
		SnapshotEntry e;
		ds >> e.stream_offset >> e.pos;
		_snapshots.append(e);
	}

	return true;
}

MarkerEntry Index::prevMarker(unsigned int from) const
{
	MarkerEntry e;
	for(int i=0;i<_markers.size();++i) {
		if(_markers[i].pos >= from)
			break;

		e = _markers[i];
	}

	if(e.pos == from)
		e = MarkerEntry();

	return e;
}

MarkerEntry Index::nextMarker(unsigned int from) const
{
	MarkerEntry e;
	for(int i=_markers.size()-1;i>=0;--i) {
		if(_markers[i].pos <= from)
			break;

		e = _markers[i];
	}

	if(e.pos == from)
		e = MarkerEntry();

	return e;
}

const IndexEntry &Index::addMarker(qint64 offset, quint32 pos, const QString &title)
{
	IndexEntry e(IDX_MARKER, 0, offset, pos, pos, 0xffffffff, title);
	e.flags = IndexEntry::FLAG_ADDED;

	// Add new entry to the index
	int idx=0;
	while(idx<_index.size()) {
		if(_index.at(idx).start >= pos)
			break;
		++idx;
	}
	_index.insert(idx, e);

	// Add entry to marker list
	int i=0;
	while(i<_markers.size()) {
		if(_markers.at(i).pos >= pos)
			break;
		++i;
	}
	_markers.insert(i, MarkerEntry(idx, pos, title));

	return _index.at(idx);
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

