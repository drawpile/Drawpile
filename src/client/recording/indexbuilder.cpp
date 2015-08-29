/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2015 Calle Laakkonen

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

#include "recording/indexbuilder.h"
#include "../shared/record/reader.h"
#include "../shared/record/writer.h"
#include "../shared/net/pen.h"
#include "../shared/net/image.h"
#include "../shared/net/meta.h"
#include "../shared/net/meta2.h"
#include "../shared/net/undo.h"
#include "../shared/net/recording.h"

#include "canvas/statetracker.h"
#include "core/layerstack.h"
#include "net/layerlist.h"

#include <QDebug>
#include <QBuffer>
#include <QColor>
#include <QBuffer>
#include <QElapsedTimer>
#include <KZip>

namespace recording {

IndexBuilder::IndexBuilder(const QString &inputfile, const QString &targetfile, QObject *parent)
	: QObject(parent), QRunnable(), _inputfile(inputfile), _targetfile(targetfile)
{
}

void IndexBuilder::abort()
{
	_abortflag = 1;
	qDebug() << "aborting indexing...";
}

void IndexBuilder::run()
{
	// Open output file
	KZip zip(_targetfile);
	if(!zip.open(QIODevice::WriteOnly)) {
		emit done(false, tr("Error opening %1 for writing").arg(_targetfile));
		return;
	}

	// Build the index
	Reader reader(_inputfile);

	Compatibility readerOk = reader.open();
	if(readerOk != COMPATIBLE && readerOk != MINOR_INCOMPATIBILITY) {
		qWarning() << "Couldn't open recording for indexing. Error code" << readerOk;
		emit done(false, reader.errorString());
		return;
	}

	MessageRecord record;
	_pos = 0;
	do {
		if(_abortflag.load()) {
			qWarning() << "Indexing aborted (index phase)";
			emit done(false, "aborted");
			return;
		}

		_offset = reader.filePosition();
		record = reader.readNext();
		if(record.status == MessageRecord::OK) {
			protocol::MessagePtr msg(record.message);
			addToIndex(msg);
		} else if(record.status == MessageRecord::INVALID) {
			qWarning() << "invalid message type" << record.error.type << "at index" << _pos;
		}
		++_pos;
	} while(record.status != MessageRecord::END_OF_RECORDING);



	// Write snapshots
	reader.rewind();
	emit progress(reader.filePosition());
	writeSnapshots(reader, zip);

	if(_abortflag.load()) {
		qWarning() << "Indexing aborted (snapshot phase)";
		emit done(false, "aborted");
		return;
	}

	// Write index
	{
		QBuffer indexBuffer;
		indexBuffer.open(QBuffer::ReadWrite);
		m_index.writeIndex(&indexBuffer);

		zip.writeFile("index", indexBuffer.data());
	}

	// Write recording hash
	QByteArray hash = hashRecording(_inputfile);
	zip.writeFile("hash", hash);

	if(!zip.close()) {
		emit done(false, tr("Error writing file"));
		return;
	}

	emit done(true, QString());
}

void IndexBuilder::writeSnapshots(Reader &reader, KZip &zip)
{
	static const qint64 SNAPSHOT_INTERVAL_MS = 1000; // snapshot interval in milliseconds
	static const int SNAPSHOT_MIN_ACTIONS = 200; // minimum number of actions between snapshots

	paintcore::LayerStack image;
	net::LayerListModel layermodel;
	canvas::StateTracker statetracker(&image, &layermodel, 1);

	MessageRecord msg;
	int snapshotCounter = 0;
	QElapsedTimer timer;
	timer.start();
	while(true) {
		if(_abortflag.load())
			return;

		msg = reader.readNext();
		if(msg.status == MessageRecord::END_OF_RECORDING)
			break;
		else if(msg.status == MessageRecord::INVALID)
			continue;

		protocol::MessagePtr m(msg.message);
		if(m->isCommand()) {
			statetracker.receiveCommand(m);
			++snapshotCounter;
		}

		// Save a snapshot every SNAPSHOT_INTERVAL or at every marker. (But no more often than SNAPSHOT_MIN_ACTIONS)
		// Note. We use the actual elapsed rendering time to decide when to snapshot. This means that (ideally),
		// the time it takes to jump to a snapshot is at most SNAPSHOT_INTERVAL milliseconds (+ the time it takes to load the snapshot)
		if(m_index.snapshots().isEmpty() || ((timer.hasExpired(SNAPSHOT_INTERVAL_MS) || m->type() == protocol::MSG_MARKER) && snapshotCounter>=SNAPSHOT_MIN_ACTIONS)) {
			qint64 streampos = reader.filePosition();
			emit progress(streampos);
			canvas::StateSavepoint sp = statetracker.createSavepoint(-1);

			QBuffer buf;
			buf.open(QBuffer::ReadWrite);
			{
				QDataStream ds(&buf);
				sp.toDatastream(ds);
			}

			int snapshotIdx = m_index.m_snapshots.size();
			zip.writeFile(QString("snapshot-%1").arg(snapshotIdx), buf.data());
			m_index.m_snapshots.append(SnapshotEntry(streampos, reader.currentIndex()));

			snapshotCounter = 0;
			timer.restart();
		}
	}
}

void IndexBuilder::addToIndex(const protocol::MessagePtr msg)
{
	IndexType type = IDX_NULL;
	QString title;
	quint32 color = _colors[msg->contextId()];

	switch(msg->type()) {
	using namespace protocol;
	case MSG_CANVAS_RESIZE: type = IDX_RESIZE; break;

	case MSG_LAYER_CREATE: type = IDX_CREATELAYER; break;

	case MSG_LAYER_DELETE: type = IDX_DELETELAYER; break;
	case MSG_PUTIMAGE: type = IDX_PUTIMAGE; break;

	case MSG_PEN_MOVE:
	case MSG_PEN_UP: type = IDX_STROKE; break;

	case MSG_TOOLCHANGE:
		_colors[msg->contextId()] = msg.cast<const protocol::ToolChange>().color();
		break;

	case MSG_ANNOTATION_CREATE:
	case MSG_ANNOTATION_DELETE:
	case MSG_ANNOTATION_EDIT:
	case MSG_ANNOTATION_RESHAPE: type = IDX_ANNOTATE; break;

	case MSG_UNDO:
		if(msg.cast<const protocol::Undo>().points() > 0)
			type = IDX_UNDO;
		else
			type = IDX_REDO;
		break;

	case MSG_FILLRECT:
		type = IDX_FILL;
		color = msg.cast<const protocol::FillRect>().color();
		break;

	case MSG_CHAT:
		type = IDX_CHAT;
		title = msg.cast<const protocol::Chat>().message().left(32);
		break;

	case MSG_INTERVAL: type = IDX_PAUSE; break;

	case MSG_MOVEPOINTER: type = IDX_LASER; break;

	case MSG_MARKER:
		type = IDX_MARKER;
		title = msg.cast<const protocol::Marker>().text();
		break;

	case MSG_USER_JOIN:
		m_index.m_ctxnames[msg->contextId()] = msg.cast<const protocol::UserJoin>().name();
		return;

	default: break;
	}

	if(type==IDX_NULL) {
		return;

	} else if(type==IDX_PUTIMAGE || type==IDX_ANNOTATE) {
		// Combine consecutive messages from the same user
		for(int i=m_index.m_index.size()-1;i>=0;--i) {
			IndexEntry &e = m_index.m_index[i];
			if(e.context_id == msg->contextId()) {
				if(e.type == type) {
					e.end = _pos;
					return;
				}
				break;
			}
		}

	} else if(type==IDX_LASER) {
		// Combine laser pointer strokes and drop other MovePointer messages
		for(int i=m_index.m_index.size()-1;i>=0;--i) {
			IndexEntry &e = m_index.m_index[i];
			if(e.context_id == msg->contextId()) {
				if(e.type == type) {
					int persistence = msg.cast<const protocol::MovePointer>().persistence();
					if(persistence==0) {
						e.flags |= IndexEntry::FLAG_FINISHED;
						return;
					} else if(!(e.flags & IndexEntry::FLAG_FINISHED)) {
						e.end = _pos;
						return;
					}
				}
				break;
			}
		}

	} else if(type==IDX_STROKE) {
		// Combine all strokes up to last pen-up from the same user
		for(int i=m_index.m_index.size()-1;i>=0;--i) {
			IndexEntry &e = m_index.m_index[i];
			if(e.context_id == msg->contextId() && e.type == IDX_STROKE) {
				if(!(e.flags & IndexEntry::FLAG_FINISHED)) {
					e.end = _pos;
					if(msg->type() == protocol::MSG_PEN_UP)
						e.flags |= IndexEntry::FLAG_FINISHED;
					return;
				}
				break;
			}
		}
	}

	// New index entry
	m_index.m_index.append(IndexEntry(type, msg->contextId(), _offset, _pos, _pos, color, title));
}

}
