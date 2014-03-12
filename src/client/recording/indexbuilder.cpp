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

#include <QDebug>
#include <QBuffer>
#include <QColor>
#include <QBuffer>

#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
#include <QSaveFile>
#else
#include <QFile>
#define QSaveFile QFile
#define NO_QSAVEFILE
#endif


#include "recording/indexbuilder.h"
#include "ora/zipwriter.h"
#include "../shared/record/reader.h"
#include "../shared/record/writer.h"
#include "../shared/net/pen.h"
#include "../shared/net/meta.h"
#include "../shared/net/recording.h"

#include "statetracker.h"
#include "core/layerstack.h"
#include "net/layerlist.h"

namespace recording {

IndexBuilder::IndexBuilder(const QString &inputfile, const QString &targetfile, QObject *parent)
	: QThread(parent), _inputfile(inputfile), _targetfile(targetfile)
{
}

void IndexBuilder::run()
{
	// Open output file
	QSaveFile savefile(_targetfile);
	if(!savefile.open(QSaveFile::WriteOnly)) {
		emit done(false, savefile.errorString());
		return;
	}

	// Build the index
	Reader reader(_inputfile);

	Compatibility readerOk = reader.open();
	if(readerOk != COMPATIBLE && readerOk != MINOR_INCOMPATIBILITY) {
		qDebug() << "Couldn't open recording for indexing. Error code" << readerOk;
		emit done(false, reader.errorString());
		return;
	}

	MessageRecord record;
	_pos = 0;
	const qint64 zero_offset = reader.position();
	do {
		_offset = reader.position() - zero_offset;
		record = reader.readNext();
		if(record.status == MessageRecord::OK) {
			protocol::MessagePtr msg(record.message);
			addToIndex(msg);
		} else if(record.status == MessageRecord::INVALID) {
			qWarning() << "invalid message type" << record.type << "at index" << _pos;
		}
		++_pos;
	} while(record.status != MessageRecord::END_OF_RECORDING);

	ZipWriter zip(&savefile);

	// Write snapshots
	reader.rewind();
	emit progress(reader.position());
	writeSnapshots(reader, zip);

	// Write index
	{
		QBuffer indexBuffer;
		indexBuffer.open(QBuffer::ReadWrite);
		_index.writeIndex(&indexBuffer);

		zip.addFile("index", indexBuffer.data());
	}

	// Write recording hash
	QByteArray hash = hashRecording(_inputfile);
	zip.addFile("hash", hash);

	zip.close();
#ifndef NO_QSAVEFILE
	savefile.commit();
#else
	savefile.close();
#endif

	emit done(true, QString());
}

void IndexBuilder::writeSnapshots(Reader &reader, ZipWriter &zip)
{
	paintcore::LayerStack image;
	net::LayerListModel layermodel;
	drawingboard::StateTracker statetracker(&image, &layermodel, 1);

	MessageRecord msg;
	while(true) {
		msg = reader.readNext();
		if(msg.status == MessageRecord::END_OF_RECORDING)
			break;
		else if(msg.status == MessageRecord::INVALID)
			continue;

		protocol::MessagePtr m(msg.message);
		if(m->isCommand())
			statetracker.receiveCommand(m);

		// TODO: should snapshot interval be adjustable or dynamic?
		if(reader.current() % 100 == 0) {
			qint64 streampos = reader.position();
			emit progress(streampos);;
			drawingboard::StateSavepoint sp = statetracker.createSavepoint();

			QBuffer buf;
			buf.open(QBuffer::ReadWrite);
			{
				QDataStream ds(&buf);
				sp.toDatastream(ds);
			}

			int snapshotIdx = _index._snapshots.size();
			zip.addFile(QString("snapshot-%1").arg(snapshotIdx), buf.data());
			_index._snapshots.append(SnapshotEntry(streampos, reader.current()));
		}
	}
}

void IndexBuilder::addToIndex(const protocol::MessagePtr msg)
{
	IndexType type = IDX_NULL;
	QString title;
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
						e._finished = true;
						return;
					} else if(!e._finished) {
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
				if(!e._finished) {
					e.end = _pos;
					if(msg->type() == protocol::MSG_PEN_UP)
						e._finished = true;
					return;
				}
				break;
			}
		}
	}

	// New index entry
	_index._index.append(IndexEntry(type, msg->contextId(), _offset, _pos, _pos, _colors[msg->contextId()], title));
}

}
