// SPDX-License-Identifier: GPL-3.0-or-later

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

	//! Generates an eight-character string, not really a ULID, but similar.
	//! Used for temporary invite codes where brevity is more important.
	static QString makeShortIdentifier();

private:
	uchar m_ulid[16];
};

Q_DECLARE_TYPEINFO(Ulid, Q_PRIMITIVE_TYPE);

#endif
