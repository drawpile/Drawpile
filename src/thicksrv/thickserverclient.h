/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#ifndef THICKSERVERCLIENT_H
#define THICKSERVERCLIENT_H

#include "../shared/server/client.h"

namespace server {

class ThickServerClient : public Client
{
public:
	ThickServerClient(QTcpSocket *socket, ServerLog *logger, QObject *parent=nullptr);
};

}

#endif
