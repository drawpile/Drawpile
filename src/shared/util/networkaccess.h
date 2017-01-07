/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2017 Calle Laakkonen

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

#ifndef NETWORKACCESS_H
#define NETWORKACCESS_H

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QString;
class QUrl;

namespace networkaccess {

/**
 * @brief Get a shared instance of a QNetworkAccessManager
 *
 * The returned instance will be unique to the current thread
 */
QNetworkAccessManager *getInstance();

/**
 * @brief A convenience wrapper around getInstance()->get()
 *
 * This aborts the request if the response mimetype differs from the expected
 *
 * @param url the URL to fetch
 * @param expectType expected mime type
 * @return network reply
 */
QNetworkReply *get(const QUrl &url, const QString &expectType);

}

#endif
