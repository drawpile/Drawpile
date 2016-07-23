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

#include "recording/indexloader.h"
#include "canvas/statetracker.h"
#include "utils/archive.h"

#include <QBuffer>
#include <KZip>
#include <QDataStream>
#include <QImage>

namespace recording {

IndexLoader::IndexLoader(const QString &recording, const QString &index)
{
	m_recordingfile = recording;
	m_file = new KZip(index);
}

IndexLoader::~IndexLoader()
{
	delete m_file;
}

bool IndexLoader::open()
{
	if(!m_file->open(QIODevice::ReadOnly))
		return false;

	// Make sure this is the right index for the recording
	QByteArray idxHash = utils::getArchiveFile(*m_file, "hash");
	QByteArray recHash = hashRecording(m_recordingfile);

	if(idxHash != recHash)
		return false;

	QByteArray indexdata = utils::getArchiveFile(*m_file, "index");
	QBuffer indexbuffer(&indexdata);
	indexbuffer.open(QBuffer::ReadOnly);
	if(!m_index.readIndex(&indexbuffer))
		return false;

	return true;
}

canvas::StateSavepoint IndexLoader::loadSavepoint(int idx, canvas::StateTracker *owner)
{
	Q_ASSERT(idx>=0 && idx<m_index.size());
	if(idx<0 || idx>=m_index.size())
		return canvas::StateSavepoint();

	Q_ASSERT((m_index.entry(idx).flags & StopEntry::HAS_SNAPSHOT));
	if(!(m_index.entry(idx).flags & StopEntry::HAS_SNAPSHOT))
		return canvas::StateSavepoint();

	QByteArray snapshotdata = utils::getArchiveFile(*m_file, QString("snapshot/%1").arg(idx));
	if(snapshotdata.isEmpty()) {
		qWarning("No data in snapshot %d", idx);
		return canvas::StateSavepoint();
	}

	QBuffer snapshotbuffer(&snapshotdata);
	snapshotbuffer.open(QBuffer::ReadOnly);
	QDataStream ds(&snapshotbuffer);

	return canvas::StateSavepoint::fromDatastream(ds, owner);
}

QImage IndexLoader::loadThumbnail(int idx)
{
	Q_ASSERT(idx>=0 && idx<m_index.thumbnails().size());

	const int stop = m_index.thumbnails().at(idx);

	QByteArray data = utils::getArchiveFile(*m_file, QString("thumbnail/%1").arg(stop));
	return QImage::fromData(data, "PNG");
}

}
