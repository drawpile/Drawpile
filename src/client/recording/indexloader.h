/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

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

#ifndef INDEXLOADER_H
#define INDEXLOADER_H

#include "index.h"

#include <QExplicitlySharedDataPointer>

class QImage;

namespace canvas {
	class StateSavepoint;
}

namespace recording {

class IndexLoader
{
public:
	IndexLoader();
	IndexLoader(const QString &recording, const QString &index, const QByteArray &expectedHash);

	IndexLoader(const IndexLoader&);
	IndexLoader &operator=(const IndexLoader&);

	~IndexLoader();

	/**
	 * @brief Open the index
	 *
	 * The list of index entries is read and loadSavepoint can then be used.
	 */
	bool open();

	//! All index entries
	QVector<IndexEntry> index() const;

	//! Entries with markers
	QVector<IndexEntry> markers() const;

	//! Entries with thumbnails
	QVector<IndexEntry> thumbnails() const;

	//! Total number of messages in the recording
	int messageCount() const;

	canvas::StateSavepoint loadSavepoint(const IndexEntry &entry);

	bool operator!() const { return !d; }
	operator bool() const { return d; }

private:
	struct Private;
	QExplicitlySharedDataPointer<Private> d;


};

}

#endif // INDEXLOADER_H
