/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
#ifndef REC_INDEX_H
#define REC_INDEX_H

#include <QVector>
#include <QHash>
#include <QString>

class QIODevice;

namespace recording {

//! Index format version
static const quint16 INDEX_VERSION = 0x0001;

enum IndexType {
	IDX_NULL,        // null entry
	IDX_MARKER,      // pause point marker
	IDX_RESIZE,      // canvas resize
	IDX_CREATELAYER, // layer creation
	IDX_DELETELAYER, // layer deletion
	IDX_PUTIMAGE,    // put image/fill area
	IDX_STROKE,      // pen stroke
	IDX_ANNOTATE,    // annotation editing
	IDX_CHAT,        // a chat message
	IDX_PAUSE,       // pause
	IDX_LASER,       // laser pointing
};

struct IndexEntry {
	IndexEntry() : type(IDX_NULL), context_id(0), offset(0), start(0), end(0), color(0), _finished(false) { }
	IndexEntry(IndexType typ, int ctx, qint64 o, int s, int e, quint32 c, const QString &title_) : type(typ), context_id(ctx), offset(o), start(s), end(e), color(c), title(title_), _finished(false) { }

	//! Type of the index entry
	IndexType type;

	//! Context ID of the message
	quint8 context_id;

	//! Offset of the first message in the file
	qint64 offset;

	//! First stream index of the action
	quint32 start;

	//! Last stream index of the action
	quint32 end;

	//! Color hint for this index entry (RGB)
	quint32 color;

	//! Title/tooltip text for the index entry
	QString title;

	// Flags used when building the index
	bool _finished;
};

struct MarkerEntry {
	MarkerEntry() : idxpos(-1), pos(0) { }
	MarkerEntry(int idxpos_, int pos_) : idxpos(idxpos_), pos(pos_) { }

	int idxpos;
	quint32 pos;
};

struct SnapshotEntry {
	SnapshotEntry() : stream_offset(-1), pos(-1) { }
	SnapshotEntry(qint64 offset, quint32 pos_) : stream_offset(offset), pos(pos_) { }

	//! Offset to the start of the snapshot in the message stream (relative to message stream block start)
	qint64 stream_offset;

	//! Stream index of the snapshot
	quint32 pos;
};

typedef QVector<IndexEntry> IndexVector;
typedef QVector<SnapshotEntry> SnapshotVector;

/**
 * Recording index
 */
class Index {
	friend class IndexBuilder;
	friend class IndexLoader;

public:

	//! Get the number of entries in the index
	int size() const { return _index.size(); }

	//! Get the name associated with the given context ID
	QString contextName(int context_id) const;

	//! Get the given index entry
	const IndexEntry &entry(int idx) const { return _index.at(idx); }

	//! Get the index entry vector
	const IndexVector &entries() const { return _index; }

	//! Get all snapshots
	const SnapshotVector &snapshots() const { return _snapshots; }

	IndexEntry nextMarker(unsigned int from) const;
	IndexEntry prevMarker(unsigned int from) const;


	bool writeIndex(QIODevice *out) const;
	bool readIndex(QIODevice *in);

private:
	IndexVector _index;
	SnapshotVector _snapshots;
	QHash<int, QString> _ctxnames;
	QVector<MarkerEntry> _markers;
};

//! Hash the recording file
QByteArray hashRecording(const QString &filename);

}

#endif
