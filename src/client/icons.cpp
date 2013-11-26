/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

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

// TODO replace this with something sensible.
// Perhaps look into http://labs.trolltech.com/blogs/2009/02/13/freedesktop-icons-in-qt/

namespace icon {

const QIcon& lock()
{
	static QIcon icon;
	if(icon.isNull()) {
		icon.addFile(":icons/lock.png", QSize(), QIcon::Normal, QIcon::On);
		icon.addFile(":icons/unlock.png", QSize(), QIcon::Normal, QIcon::Off);
	}
	return icon;
}

const QIcon& kick()
{
	static QIcon icon;
	if(icon.isNull()) {
		icon.addFile(":icons/kick.png", QSize(), QIcon::Normal, QIcon::On);
	}
	return icon;
}

const QIcon& remove()
{
	static QIcon icon;
	if(icon.isNull()) {
		icon.addFile(":icons/list-remove.png", QSize(), QIcon::Normal, QIcon::On);
	}
	return icon;
}

const QIcon& layervisible()
{
	static QIcon icon;
	if(icon.isNull()) {
		icon.addFile(":icons/layer-visible.png", QSize(), QIcon::Normal, QIcon::On);
	}
	return icon;
}

const QIcon& add()
{
	static QIcon icon;
	if(icon.isNull()) {
		icon.addFile(":icons/list-add.png", QSize(), QIcon::Normal, QIcon::On);
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

