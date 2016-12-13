/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#ifndef SESSIONDESC_H
#define SESSIONDESC_H

#include "../net/protover.h"

#include <QString>
#include <QMetaType>
#include <QList>
#include <QHostAddress>
#include <QDateTime>
#include <QUuid>

namespace server {

class Session;
class Client;

//! Return a properly formatted session ID string
QString sessionIdString(const QUuid &id);

//! Is the given string a valid session ID alias?
bool isValidSessionAlias(const QString &alias);

/**
 * @brief Information about an available session
 */
struct SessionDescription {
	QUuid id;
	QString alias;
	protocol::ProtocolVersion protocolVersion;
	int userCount;
	int maxUsers;
	QString title;
	QByteArray passwordHash;
	QString founder;
	bool closed;
	bool persistent;
	bool nsfm;
	QDateTime startTime;

	SessionDescription();
	SessionDescription(const Session &session);
};

}

Q_DECLARE_METATYPE(server::SessionDescription)

#endif

