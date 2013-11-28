/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2009-2013 Calle Laakkonen

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
#ifndef ORAREADER_H
#define ORAREADER_H

#include <QString>
#include "../shared/net/message.h"

class Zipfile;
class QDomElement;
class QPoint;

namespace openraster {

/**
 * An OpenRaster reader.
 */
class Reader {
	public:
		enum Warning {
			//! No warnings (i.e. DrawPile supports all features of the file)
			NO_WARNINGS = 0,
			//! The OpenRaster file uses unsupported app. specific extensions
			ORA_EXTENDED = 0x01,
			//! Nested layers are used
			ORA_NESTED = 0x02
		};
		Q_DECLARE_FLAGS(Warnings, Warning)

		Reader();
		Reader(const Reader&) = delete;

		//! Load the image
		bool load(const QString &filename);

		/**
		 * @brief get the session initialization commands
		 *
		 * Note. The command list is only valid if load() has been called
		 * beforehand and the return value was true.
		 *
		 * @return session initialization command list
		 */
		const QList<protocol::MessagePtr> &initCommands() const { return _commands; }

		//! Get the error message
		const QString& error() const { return _error; }

		//! Get the warning flags
		const Warnings warnings() const { return _warnings; }

	private:
		bool loadLayers(Zipfile &zip, const QDomElement& stack, QPoint offset);
		void loadAnnotations(const QDomElement& annotations);

		QString _error;
		Warnings _warnings;
		QList<protocol::MessagePtr> _commands;
		int _layerid;
		int _annotationid;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Reader::Warnings)

}

#endif

