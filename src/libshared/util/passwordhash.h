/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

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

#ifndef PASSWORDHASH_H
#define PASSWORDHASH_H

#include <QtGlobal>
#include "libshared/util/qtcompat.h"

class QByteArray;
class QString;

namespace server {
namespace passwordhash {

enum Algorithm {
	PLAINTEXT,
	SALTED_SHA1, // deprecated
	PBKDF2,
	SODIUM, // the best algorithm offered by libsodium
	BEST_ALGORITHM = 255,
};


/**
 * @brief Check the given password against the hash
 * @param password
 * @param hash
 * @return true if password matches
 */
bool check(const QString &password, const QByteArray &hash);

/**
 * @brief Generate a password hash
 *
 * If an empty string is given, the returned hash is also empty.
 *
 * @param password the password to hash
 * @param algorithm the algorithm to use
 * @return password hash that can be given to the check function
 */
QByteArray hash(const QString &password, Algorithm algorithm=BEST_ALGORITHM);

/**
 * @brief Check if the given password hash is valid and uses a supported algorithm.
 */
bool isValidHash(const QByteArray &hash);

}
}

#endif // PASSWORDHASH_H

