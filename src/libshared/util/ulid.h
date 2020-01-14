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

#ifndef UTILS_ULID_H
#define UTILS_ULID_H

#include <QString>

class QDateTime;

/**
 * @brief Universally Unique Lexicographically Sortable Identifier
 */
class Ulid {
public:
	//! Construct a null ULID
	Ulid();

	/**
	 * @brief Parse a string encoded ULID
	 * If the string does not contain a valid ULID, the
	 * newly constructed object will be a null ULID.
	 *
	 * @param str
	 */
	explicit Ulid(const QString &str);

	//! Construct a new random ULID (using the current time)
	static Ulid make();

	//! Construct a new random ULID for the given timestamp
	static Ulid make(const QDateTime &timestamp);

	//! Is this a null (invalid) ULID
	bool isNull() const;

	//! Extract the timestamp portion
	QDateTime timestamp() const;

	//! Encode into a 26 character long base32 string
	QString toString() const;

	bool operator==(const Ulid &other) const;
	bool operator!=(const Ulid &other) const { return !(*this == other); }
	bool operator<(const Ulid &other) const;
	bool operator>(const Ulid &other) const;

private:
	uchar m_ulid[16];
};

Q_DECLARE_TYPEINFO(Ulid, Q_PRIMITIVE_TYPE);

#endif
