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

#ifndef NETWORKACCESS_H
#define NETWORKACCESS_H

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QString;
class QUrl;
class QImage;

namespace widgets {
	class NetStatus;
}

namespace networkaccess {

/**
 * @brief Get a shared instance of a QNetworkAccessManager
 */
QNetworkAccessManager *getInstance();

/**
 * @brief A convenience wrapper around getInstance()->get()
 *
 *
 * @param url the URL to fetch
 * @param expectType expected mime type
 * @param netstatus the status widget whose progress meter to update
 * @return network reply
 */
QNetworkReply *get(const QUrl &url, const QString &expectType, widgets::NetStatus *netstatus);

/**
 * @brief A convenience wrapepr aaround get() that expects an image in response
 *
 * If an error occurs, the callback is called with a null image and an error message.
 *
 * @param url the URL to fetch
 * @param netstatus the status widget whose progress meter to update
 * @param callback the callback to call with the returned image or error message
 */
void getImage(const QUrl &url, widgets::NetStatus *netstatus, std::function<void(const QImage &image, const QString &errorMsg)> callback);

}

#endif // NETWORKACCESS_H
