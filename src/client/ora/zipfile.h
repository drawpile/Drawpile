/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2009 Calle Laakkonen

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
#ifndef ZIPFILE_H
#define ZIPFILE_H

#include <QString>

class QIODevice;

struct ZipfileImpl;

class Zipfile {
	public:
		enum Mode {READ, WRITE, OVERWRITE};
		enum Method {DEFAULT, STORE, DEFLATE};

		//! Open a file 
		Zipfile(const QString& path, Mode mode);
	
		//! Destructor
		~Zipfile();

		//! Open the file
		bool open();

		//! Close the file
		bool close();

		//! Get the latest error message
		const char *errorMessage() const;

		//! Add a file to the zip archive from the given IO source
		bool addFile(const QString& name, QIODevice *source, unsigned int length, time_t modified, Method method=DEFAULT);

		//! Get the named file from the arcive
		QIODevice *getFile(const QString& name);

		//! Get the file at the given index from the arcive
		QIODevice *getFile(int index);

	private:
		ZipfileImpl *priv;
};

#endif
