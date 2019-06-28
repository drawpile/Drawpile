/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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
#ifndef WEBMEXPORTER_H
#define WEBMEXPORTER_H

#include "videoexporter.h"

class WebmExporter : public VideoExporter
{
	Q_OBJECT
public:
	WebmExporter(QObject *parent=nullptr);
	~WebmExporter();

	void setFilename(const QString &filename);

protected:
	void initExporter() override;
	void startExporter() override;
	void writeFrame(const QImage &image, int repeat) override;
	void shutdownExporter() override;

private:
	struct Private;
	Private *d;
};

#endif

