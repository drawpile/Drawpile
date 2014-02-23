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

#include "recording/indexloader.h"
#include "ora/zipreader.h"
#include "statetracker.h"

namespace recording {

IndexLoader::IndexLoader(const QString &recording, const QString &index) : _file(0)
{
	_recordingfile = recording;
	_file = new ZipReader(index);
}

IndexLoader::~IndexLoader()
{
	delete _file;
}

bool IndexLoader::open()
{
	if(!_file->isReadable())
		return false;

	// Make sure this is the right index for the recording
	QByteArray idxHash = _file->fileData("hash");
	QByteArray recHash = hashRecording(_recordingfile);

	if(idxHash != recHash)
		return false;

	QByteArray indexdata = _file->fileData("index");
	QBuffer indexbuffer(&indexdata);
	indexbuffer.open(QBuffer::ReadOnly);
	if(!_index.readIndex(&indexbuffer))
		return false;

	return true;
}

drawingboard::StateSavepoint IndexLoader::loadSavepoint(int idx, drawingboard::StateTracker *owner)
{
	if(idx<0 || idx>=_index.snapshots().size())
		return drawingboard::StateSavepoint();

	QByteArray snapshotdata = _file->fileData(QString("snapshot-%1").arg(idx));
	if(snapshotdata.isEmpty()) {
		qWarning() << "no snapshot" << idx << "data!";
		return drawingboard::StateSavepoint();
	}

	QBuffer snapshotbuffer(&snapshotdata);
	snapshotbuffer.open(QBuffer::ReadOnly);
	QDataStream ds(&snapshotbuffer);

	return drawingboard::StateSavepoint::fromDatastream(ds, owner);
}

}
