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
#ifndef IMAGESERIESEXPORTER_H
#define IMAGESERIESEXPORTER_H

#include <QString>
#include <QSize>

#include "videoexporter.h"

class ImageSeriesExporter : public VideoExporter
{
public:
	void setOutputPath(const QString &path) { _path = path; }
	void setFilePattern(const QString &pattern) { _filepattern = pattern; }
	void setFormat(const QString &format) { _format = format.toLatin1(); }

	void finish();

protected:
	bool writeFrame(const QImage &image);

private:
	QString _path;
	QString _filepattern;
	QByteArray _format;
};

#endif // IMAGESERIESEXPORTER_H
