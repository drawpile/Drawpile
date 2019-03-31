/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2009-2019 Calle Laakkonen

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
#ifndef ORAREADER_H
#define ORAREADER_H

#include "../shared/net/message.h"

#include <QString>

class QPixmap;

namespace openraster {

struct OraResult {
	enum Warning {
		//! No warnings (i.e. Drawpile supports all features of the file)
		NO_WARNINGS = 0,
		//! The OpenRaster file uses unsupported app. specific extensions
		ORA_EXTENDED = 0x01,
		//! Nested layers are used
		ORA_NESTED = 0x02,
		//! Unsupported background tile size or format
		UNSUPPORTED_BACKGROUND_TILE = 0x04
	};
	Q_DECLARE_FLAGS(Warnings, Warning)

	//! The commands to initialize a canvas
	protocol::MessageList commands;

	//! Error message (empty if file was loaded succesfully)
	QString error;

	//! Warning flags
	Warnings warnings;

	//! Image resolution info (presently not used internally by Drawpile)
	int dpiX, dpiY;

	OraResult() : warnings(NO_WARNINGS) { }
	OraResult(const QString &error) : error(error) { }
};

Q_DECLARE_OPERATORS_FOR_FLAGS(OraResult::Warnings)

//! Load an OpenRaster image
OraResult loadOpenRaster(const QString &filename);

//! Get the thumbnail from an ORA file
QImage loadOpenRasterThumbnail(const QString &filename);

}

#endif

