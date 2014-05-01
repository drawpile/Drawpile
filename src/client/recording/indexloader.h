/*
   DrawPile - a collaborative drawing program.

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

#ifndef INDEXLOADER_H
#define INDEXLOADER_H

#include "index.h"

class ZipReader;

namespace drawingboard {
	class StateSavepoint;
	class StateTracker;
}

namespace recording {

class IndexLoader
{
public:
	IndexLoader(const QString &recording, const QString &index);
	IndexLoader(const IndexLoader&) = delete;
	IndexLoader &operator=(const IndexLoader&) = delete;
	~IndexLoader();

	bool open();

	Index &index() { return _index; }

	drawingboard::StateSavepoint loadSavepoint(int idx, drawingboard::StateTracker *owner);

private:
	QString _recordingfile;
	ZipReader *_file;
	Index _index;
};

}

#endif // INDEXLOADER_H
