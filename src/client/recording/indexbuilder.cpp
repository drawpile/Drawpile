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

#include "recording/indexbuilder.h"
#include "../shared/record/reader.h"
#include "../shared/record/writer.h"
#include "../shared/net/pen.h"
#include "../shared/net/image.h"
#include "../shared/net/meta.h"
#include "../shared/net/undo.h"
#include "../shared/net/recording.h"

#include "statetracker.h"
#include "core/layerstack.h"
#include "net/layerlist.h"

#include <QDebug>
#include <QBuffer>
#include <QColor>
#include <QBuffer>
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
		_index.writeIndex(&indexBuffer);

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
	static const int SNAPSHOT_INTERVAL = 200;

	paintcore::LayerStack image;
	net::LayerListModel layermodel;
	drawingboard::StateTracker statetracker(&image, &layermodel, 1);

	MessageRecord msg;
	while(true) {
		if(_abortflag.load())
			return;

		msg = reader.readNext();
		if(msg.status == MessageRecord::END_OF_RECORDING)
			break;
		else if(msg.status == MessageRecord::INVALID)
			continue;

		protocol::MessagePtr m(msg.message);
		if(m->isCommand())
			statetracker.receiveCommand(m);

		// TODO: should snapshot interval be adjustable or dynamic?
		if(reader.currentIndex() % SNAPSHOT_INTERVAL == 0) {
			qint64 streampos = reader.filePosition();
			emit progress(streampos);;
			drawingboard::StateSavepoint sp = statetracker.createSavepoint();

			QBuffer buf;
			buf.open(QBuffer::ReadWrite);
			{
				QDataStream ds(&buf);
				sp.toDatastream(ds);
			}

			int snapshotIdx = _index._snapshots.size();
			zip.writeFile(QString("snapshot-%1").arg(snapshotIdx), buf.data());
			_index._snapshots.append(SnapshotEntry(streampos, reader.currentIndex()));
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

	case MSG_LAYER_CREATE:
	case MSG_LAYER_COPY: type = IDX_CREATELAYER; break;

	case MSG_LAYER_DELETE: type = IDX_DELETELAYER; break;
	case MSG_PUTIMAGE: type = IDX_PUTIMAGE; break;

	case MSG_PEN_MOVE:
	case MSG_PEN_UP: type = IDX_STROKE; break;

	case MSG_TOOLCHANGE:
		_colors[msg->contextId()] = msg.cast<const protocol::ToolChange>().color_h();
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
		_index._ctxnames[msg->contextId()] = msg.cast<const protocol::UserJoin>().name();
		return;

	default: break;
	}

	if(type==IDX_NULL) {
		return;

	} else if(type==IDX_PUTIMAGE || type==IDX_ANNOTATE) {
		// Combine consecutive messages from the same user
		for(int i=_index._index.size()-1;i>=0;--i) {
			IndexEntry &e = _index._index[i];
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
		for(int i=_index._index.size()-1;i>=0;--i) {
			IndexEntry &e = _index._index[i];
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
		for(int i=_index._index.size()-1;i>=0;--i) {
			IndexEntry &e = _index._index[i];
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
	_index._index.append(IndexEntry(type, msg->contextId(), _offset, _pos, _pos, color, title));
}

}
