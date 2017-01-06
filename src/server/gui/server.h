/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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

#ifndef SERVER_H
#define SERVER_H

#include <QObject>

namespace server {
namespace gui {

/**
 * @brief Abstract base class for server connectors
 */
class Server : public QObject
{
	Q_OBJECT
public:
	explicit Server(QObject *parent=nullptr);

	/**
	 * @brief Is this the local server?
	 */
	virtual bool isLocal() const = 0;

	/**
	 * @brief Get the server's address
	 *
	 * For local servers, this is the either the manually specified announcement
	 * address or a buest guess.
	 */
	virtual QString address() const = 0;

	/**
	 * @brief Get the port the server is (or will be) running on
	 */
	virtual int port() const = 0;
};

}
}

#endif
