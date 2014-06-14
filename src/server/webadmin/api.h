/*
   DrawPile - a collaborative drawing program.

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

#ifndef WEB_ADMIN_API_H
#define WEB_ADMIN_API_H

class MicroHttpd;

namespace server {

class SessionServer;

/**
 * @brief Add all the web API request handlers
 *
 * @param httpServer
 * @param sessionServer
 */
void initWebAdminApi(MicroHttpd *httpServer, SessionServer *sessionServer);

}

#endif
