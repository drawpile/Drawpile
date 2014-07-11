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

#include "recording/indexloader.h"
#include "statetracker.h"
#include "utils/archive.h"

#include <QDebug>
#include <QBuffer>
#include <KZip>

namespace recording {

IndexLoader::IndexLoader(const QString &recording, const QString &index) : _file(0)
{
	_recordingfile = recording;
	_file.reset(new KZip(index));
}

IndexLoader::~IndexLoader()
{
}

bool IndexLoader::open()
{
	if(!_file->open(QIODevice::ReadOnly))
		return false;

	// Make sure this is the right index for the recording
	QByteArray idxHash = utils::getArchiveFile(*_file, "hash");
	QByteArray recHash = hashRecording(_recordingfile);

	if(idxHash != recHash)
		return false;

	QByteArray indexdata = utils::getArchiveFile(*_file, "index");
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

	QByteArray snapshotdata = utils::getArchiveFile(*_file, QString("snapshot-%1").arg(idx));
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
