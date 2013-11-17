/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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
#ifndef DP_SESSION_LOADER_H
#define DP_SESSION_LOADER_H

#include <QSize>
#include <QColor>
#include <QString>

namespace net {
	class Client;
}

/**
 * \brief Base class for session initializers.
 * 
 * Note. The initializers make two assumptions: they are running in
 * loopback mode and that the session has not already been initialized.
 * 
 * These assumptions allow the initializers to pick the initial layer/annotation
 * IDs.
 */
class SessionLoader {
public:
	virtual ~SessionLoader() = default;
	
	virtual bool sendInitCommands(net::Client *client) const = 0;
};

class BlankCanvasLoader : public SessionLoader {
public:
	BlankCanvasLoader(const QSize &size, const QColor &color) : _size(size), _color(color)
	{}
	
	bool sendInitCommands(net::Client *client) const;
	
private:
	QSize _size;
	QColor _color;
};

class ImageCanvasLoader : public SessionLoader{
public:
	ImageCanvasLoader(const QString &filename) : _filename(filename) {}
	
	bool sendInitCommands(net::Client *client) const;
	
private:
	QString _filename;
};

#endif
