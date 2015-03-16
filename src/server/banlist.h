/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#ifndef BANLIST_H
#define BANLIST_H

#include "updateablefile.h"

#include <QList>
#include <QHostAddress>

namespace server {

typedef QPair<QHostAddress, int> BannedIP; // <address, subnet mask>

/**
 * @brief List of IP addresses banned from this server
 */
class BanList : public QObject
{
	Q_OBJECT
public:
	explicit BanList(QObject *parent = 0);

	void setPath(const QString &path) { _file.setPath(path); }

	bool isBanned(const QHostAddress &address);

private:
	void reloadBanList();

	QList<BannedIP> _banlist;
	UpdateableFile _file;
};

}

#endif
