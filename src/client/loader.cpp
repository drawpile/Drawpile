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

#include <QImage>
#include "loader.h"
#include "net/client.h"

namespace {
bool loadSingleLayerImage(net::Client *client, const QString &filename)
{
	QImage image;
	if(image.load(filename)==false)
		return false;
	image = image.convertToFormat(QImage::Format_ARGB32);
	
	client->sendCanvasResize(image.size());
	client->sendNewLayer(1, Qt::transparent, "Background");
	client->sendImage(1, 0, 0, image, false);
	return true;
}

}

bool BlankCanvasLoader::sendInitCommands(net::Client *client) const
{
	client->sendCanvasResize(_size);
	client->sendNewLayer(1, _color, "Background");
	return true;
}

bool ImageCanvasLoader::sendInitCommands(net::Client *client) const
{
	// TODO ORA loading
	return loadSingleLayerImage(client, _filename);
}