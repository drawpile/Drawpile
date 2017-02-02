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

#include "passwordhash.h"

#include <QString>
#include <QCryptographicHash>
#include <QList>

namespace server {
namespace passwordhash {

QByteArray makesalt(int len)
{
	// TODO use a secure random number generator
	QByteArray salt(len, 0);
	for(int i=0;i<len;++i)
		salt[i] = qrand() % 255;

	return salt;
}

QByteArray saltedSha1(const QString &password, const QByteArray &salt)
{
	QCryptographicHash h(QCryptographicHash::Sha1);
	h.addData(salt);
	h.addData(password.toUtf8());
	return h.result();
}

bool isValidHash(const QByteArray &hash)
{
	if(hash.startsWith("*"))
		return true;

	const int sep = hash.indexOf(';');
	if(sep<0)
		return false;

	if(hash.startsWith("plain;")) {
		return hash.length() > 6;

	} else if(hash.startsWith("s+sha1")) {
		// There should be two ; characters (three is right out)
		const int sep2 = hash.indexOf(';', sep+1);
		if(sep2<0)
			return false;
		const int sep3 = hash.indexOf(';', sep2+1);
		return sep3 < 0;

	} else
		return false;
}

bool check(const QString &password, const QByteArray &hash)
{
	// Special case: no password set
	if(password.isEmpty() && hash.isEmpty())
		return true;

	if(!isValidHash(hash))
		return false;

	QList<QByteArray> parts = hash.split(';');

	if(parts.at(0) == "*") {
		// disabled password
		return false;

	} else if(parts.at(0) == "plain") {
		QString pw = QString::fromUtf8(parts.at(1));
		return password == pw;

	} else if(parts.at(0) == "s+sha1") {
		QByteArray hp = saltedSha1(password, QByteArray::fromHex(parts.at(1)));
		return hp == QByteArray::fromHex(parts.at(2));
	}

	// unsupported algorithm
	return false;
}

QByteArray hash(const QString &password, Algorithm algorithm)
{
	if(password.isEmpty())
		return QByteArray();

	QByteArray salt = makesalt(16);

	// TODO support real password hashing algorithms, like PBKDF2
	switch(algorithm) {
	case PLAINTEXT: return ("plain;" + password).toUtf8();
	case SALTED_SHA1: return "s+sha1;" + salt.toHex() + ";" + saltedSha1(password, salt).toHex();
	}

	return QByteArray();
}

}
}
