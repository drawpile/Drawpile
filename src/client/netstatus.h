/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

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
#ifndef NETSTATUS_H
#define NETSTATUS_H

#include <QLabel>
#include <QPixmap>

namespace widgets {
//! Network connection status widget
/**
 * This widget displays the current status of the connection with the server
 * and the address of the host.
 */
class NetStatus : public QWidget
{
	Q_OBJECT
	public:
		NetStatus(QWidget *parent);
	
	public slots:
		//! Connect to a host
		void connectHost(const QString& address);
		//! Disconnect from host
		void disconnectHost();
		//! Copy the address to clipboard
		void copyAddress();

	private:
		QLabel *label_, *icon_;
		QPixmap offlineicon_;
		QString address_;
		QAction *copyaction_;
};

}

#endif

