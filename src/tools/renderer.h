/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

#ifndef DPCMD_RENDERER_H
#define DPCMD_RENDERER_H

#include <QString>
#include <QSize>

enum class ExportEvery {
	Message,
	Sequence
};

struct DrawpileCmdSettings {
	QString inputFilename;
	QString outputFilePattern;
	QByteArray outputFormat;
	int exportEveryN;
	ExportEvery exportEveryMode;

	QSize maxSize;

	bool fixedSize;
	bool mergeAnnotations;
	bool verbose;
	bool acl;
};

bool renderDrawpileRecording(const DrawpileCmdSettings &settings);

#endif

