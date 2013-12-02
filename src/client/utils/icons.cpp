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

#include <QIcon>

#include "icons.h"

namespace icon {

const QIcon& layerHide()
{
	static QIcon icon;
	if(icon.isNull()) {
		icon.addFile(":icons/eye_closed.png", QSize(), QIcon::Normal, QIcon::On);
		icon.addFile(":icons/eye_open.png", QSize(), QIcon::Normal, QIcon::Off);
	}
	return icon;
}


const QIcon& lock()
{
	static QIcon icon;
	if(icon.isNull()) {
		icon.addFile(":icons/lock_closed.png", QSize(), QIcon::Normal, QIcon::On);
		icon.addFile(":icons/lock_open.png", QSize(), QIcon::Normal, QIcon::Off);
	}
	return icon;
}

const QIcon& network()
{
	static QIcon icon;
	if(icon.isNull()) {
		icon.addFile(":icons/network-idle.png", QSize(), QIcon::Normal, QIcon::On);
		icon.addFile(":icons/network-offline.png", QSize(), QIcon::Normal, QIcon::Off);
	}
	return icon;
}

const QIcon& network_transmit()
{
	static QIcon icon;
	if(icon.isNull())
		icon.addFile(":icons/network-transmit.png");
	return icon;
}

const QIcon& network_receive()
{
	static QIcon icon;
	if(icon.isNull())
		icon.addFile(":icons/network-receive.png");
	return icon;
}

const QIcon& network_transmit_receive()
{
	static QIcon icon;
	if(icon.isNull())
		icon.addFile(":icons/network-transmit-receive.png");
	return icon;
}

}

