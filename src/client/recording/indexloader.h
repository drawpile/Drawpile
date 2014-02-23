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

	const Index &index() const { return _index; }

	drawingboard::StateSavepoint loadSavepoint(int idx, drawingboard::StateTracker *owner);

private:
	QString _recordingfile;
	ZipReader *_file;
	Index _index;
};

}

#endif // INDEXLOADER_H
