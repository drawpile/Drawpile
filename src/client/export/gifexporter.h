/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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
#ifndef GIFEXPORTER_H
#define GIFEXPORTER_H

#include "videoexporter.h"

class GifExporter : public VideoExporter
{
	Q_OBJECT
public:
	enum DitheringMode { DIFFUSE, ORDERED, THRESHOLD };

	GifExporter(QObject *parent=0);
	~GifExporter();

	void setFilename(const QString &path);
	void setDithering(DitheringMode mode);
	void setOptimize(bool optimze);

protected:
	void initExporter();
	void startExporter();
	void writeFrame(const QImage &image, int repeat);
	void shutdownExporter();

private:
	struct Private;
	Private *p;
};

#endif

