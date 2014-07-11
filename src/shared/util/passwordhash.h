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

#ifndef PASSWORDHASH_H
#define PASSWORDHASH_H

class QByteArray;
class QString;

namespace server {
namespace passwordhash {

enum Algorithm {
	SALTED_SHA1
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
 * @param password the password to hash
 * @param algorithm the algorithm to use
 * @return
 */
QByteArray hash(const QString &password, Algorithm algorithm=SALTED_SHA1);

}
}

#endif // PASSWORDHASH_H

