/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2019 Calle Laakkonen

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

#include "config.h"
#include "paths.h"

#include <QGuiApplication>
#include <QDir>

namespace utils {
namespace paths {

QStringList dataPaths()
{
	static QStringList datapaths;

	if(datapaths.isEmpty()) {
#if defined(Q_OS_MAC)
		datapaths << QStandardPaths::writableLocation(QStandardPaths::DataLocation);
		datapaths << QDir(qApp->applicationDirPath() + QStringLiteral("/../Resources")).absolutePath();

#else
		datapaths << qApp->applicationDirPath();
		datapaths << QStandardPaths::standardLocations(QStandardPaths::DataLocation);
#endif
	}
	return datapaths;
}

QString locateDataFile(const QString &filename)
{
	for(const QString &datapath : dataPaths()) {
		const QDir d { datapath };
		if(d.exists(filename))
			return d.filePath(filename);
	}
	return QString();
}

QString writablePath(QStandardPaths::StandardLocation location, const QString &dirOrFileName, const QString &filename)
{
	QString path = QStandardPaths::writableLocation(location);

	// Setting both dirOrFilename and filename means dirOrFilename is treated
	// as a directory name and must be created and filename is the name of the actual file
	// Both can be ".", in which case the writableLocation is returned as is, but the path is still created.
	if(!filename.isEmpty())
		QDir(path).mkpath(dirOrFileName);

	if(!dirOrFileName.isEmpty() && dirOrFileName != ".") {
		if(!path.endsWith('/'))
			path.append('/');
		path += dirOrFileName;
	}

	if(!filename.isEmpty() && filename != ".") {
		if(!path.endsWith('/'))
			path.append('/');
		path += filename;
	}

	return path;
}

}
}
