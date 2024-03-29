// SPDX-License-Identifier: GPL-3.0-or-later

#include "libshared/util/passwordhash.h"
#include "libshared/util/qtcompat.h"

#include <QList>
#include <QString>
#include <QCryptographicHash>
#ifdef HAVE_QT_COMPAT_PBKDF2
#include <QPasswordDigestor>
#endif
#include <QRandomGenerator>

#ifdef HAVE_LIBSODIUM
#include <sodium.h>
#endif

namespace server {
namespace passwordhash {

static QByteArray makesalt()
{
	quint32 buf[8];
	QRandomGenerator::global()->fillRange(buf);
	return QByteArray(reinterpret_cast<const char*>(buf), sizeof buf);
}

static QByteArray saltedSha1(const QString &password, const QByteArray &salt)
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

	if(hash.startsWith("plain;")) {
		return hash.length() > 6;

	} else if(hash.startsWith("s+sha1;")) {
		return hash.count(';') == 2;
	} else if(hash.startsWith("pbkdf2;")) {
#ifdef HAVE_QT_COMPAT_PBKDF2
		return hash.count(';') == 3;
#else
		qWarning("Qt 5.12 needed to support PBKDF2 hashes!");
		return false;
#endif
	} else if(hash.startsWith("sodium;")) {
#ifdef HAVE_LIBSODIUM
		return hash.length() > 7 && uint(hash.length()) < 7+crypto_pwhash_STRBYTES;
#else
		qWarning("Libsodium needed to support Argon2 hashes!");
		return false;
#endif
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

	if(hash[0] == '*') {
		// disabled password
		return false;

	} else if(hash.startsWith("plain;")) {
		const QString pw = QString::fromUtf8(hash.mid(6));
		return password == pw;

	} else if(hash.startsWith("s+sha1;")) {
		const auto parts = hash.split(';');
		const QByteArray hp = saltedSha1(password, QByteArray::fromHex(parts.at(1)));
		return hp == QByteArray::fromHex(parts.at(2));

	} else if(hash.startsWith("pbkdf2;")) {
#ifdef HAVE_QT_COMPAT_PBKDF2
		const auto parts = hash.split(';');
		const auto salt = QByteArray::fromBase64(parts.at(2));
		const auto expected = QByteArray::fromBase64(parts.at(3));
		if(parts.at(1) == "1") { // version tag
			const auto actual = QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha512, password.toUtf8(), salt, 20000, 64);
			return actual == expected;
		}
#endif
	} else if(hash.startsWith("sodium;")) {
#ifdef HAVE_LIBSODIUM
		const QByteArray passwd = password.toUtf8();
		return crypto_pwhash_str_verify(
			hash.constData()+7,
			passwd.constData(),
			passwd.length()
			) == 0;
#endif
	}

	// unsupported algorithm
	return false;
}

namespace {

QByteArray hashPlaintext(const QString &password)
{
	return ("plain;" + password).toUtf8();
}

QByteArray hashSaltedSha1(const QString &password)
{
	// Note: no longer generate this type of hash when minimum supported Qt version is 5.12
	const QByteArray salt = makesalt();
	return "s+sha1;" + salt.toHex() + ";" + saltedSha1(password, salt).toHex();
}

#ifdef HAVE_QT_COMPAT_PBKDF2
QByteArray hashPbkdf2(const QString &password)
{
	const QByteArray salt = makesalt();
	return "pbkdf2;1;" +
		salt.toBase64() + ";" +
		QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha512, password.toUtf8(), salt, 20000, 64).toBase64()
		;
}
#endif

#ifdef HAVE_LIBSODIUM
QByteArray hashSodium(const QString &password)
{
	const QByteArray passwd = password.toUtf8();
	QByteArray hashed(crypto_pwhash_STRBYTES, 0);
	if(crypto_pwhash_str(
		hashed.data(),
		passwd.constData(),
		passwd.length(),
		crypto_pwhash_OPSLIMIT_INTERACTIVE,
		crypto_pwhash_MEMLIMIT_INTERACTIVE
		))
	{
		qWarning("crypto_pwhash_str out of memory!");
		return QByteArray();
	}
	const int nullbyte = hashed.indexOf('\0');
	if(nullbyte>=0)
		hashed.truncate(nullbyte);
	return "sodium;" + hashed;
}
#endif

}

QByteArray hash(const QString &password, Algorithm algorithm)
{
	if(password.isEmpty())
		return QByteArray();

	switch(algorithm) {
	case BEST_ALGORITHM:
#ifdef HAVE_LIBSODIUM
		return hashSodium(password);
#elif defined(HAVE_QT_COMPAT_PBKDF2)
		return hashPbkdf2(password);
#else
		return hashSaltedSha1(password);
#endif
	case PLAINTEXT:
		return hashPlaintext(password);
	case SALTED_SHA1:
		return hashSaltedSha1(password);
	case PBKDF2:
#ifdef HAVE_QT_COMPAT_PBKDF2
		return hashPbkdf2(password);
#endif
	case SODIUM:
#ifdef HAVE_LIBSODIUM
		return hashSodium(password);
#else
		break;
#endif
	}

	return QByteArray();
}

}
}
