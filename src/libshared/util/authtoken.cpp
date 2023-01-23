/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018-2019 Calle Laakkonen

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

#include "libshared/util/authtoken.h"

#include <QList>
#include <QJsonDocument>
#include <QJsonObject>

#include <sodium.h>

namespace server {

AuthToken::AuthToken()
	: m_version(0)
{
}

AuthToken::AuthToken(const QByteArray &data)
	: m_version(0)
{
	QList<QByteArray> components = data.split('.');
	if(components.size() < 1)
		return;

	bool ok;
	m_version = components.at(0).toInt(&ok);
	if(!ok)
		return;

	if(m_version == 1 && components.size() == 3) {
		m_payload = components.at(1);
		m_signature = QByteArray::fromBase64(components.at(2));
	} else if(m_version == 2 && components.size() == 4) {
		m_payload = components.at(1);
		m_avatar = components.at(2);
		m_signature = QByteArray::fromBase64(components.at(3));
	}
}

bool AuthToken::isValid() const
{
	return
		(m_version == 1 || m_version == 2) &&
		!m_payload.isEmpty() &&
		m_signature.length() == crypto_sign_BYTES
		;
}

bool AuthToken::checkSignature(const QByteArray &pubkey) const
{
	if(pubkey.length() != crypto_sign_PUBLICKEYBYTES)
		return false;

	if(!isValid())
		return false;

	if(sodium_init() < 0) {
		qCritical("Libsodium couldn't be initialized!");
		return false;
	}

	QByteArray msg = QByteArray::number(m_version) + "." + m_payload;

	if(m_version == 2) {
		msg += ".";
		msg += m_avatar;
	}

	const int ret = crypto_sign_ed25519_verify_detached(
		reinterpret_cast<const unsigned char*>(m_signature.constData()),
		reinterpret_cast<const unsigned char*>(msg.constData()),
		msg.length(),
		reinterpret_cast<const unsigned char*>(pubkey.constData())
		);

	return ret == 0;
}

QJsonObject AuthToken::payload() const
{
	return QJsonDocument::fromJson(QByteArray::fromBase64(m_payload)).object();
}

bool AuthToken::validatePayload(const QString &groupId, quint64 nonce) const
{
	const QJsonObject o = payload();

	// The most important check first: the nonce must match whatwe expected.
	bool ok;
	const quint64 n = o["nonce"].toString().toULongLong(&ok, 16);
	if(!ok || n != nonce)
		return false;

	// TODO check issued-at-time too? (remember to allow for clock skew)

	// Username shouldn't be empty. If we're being really strict,
	// we should also check that the name meets the server's
	// validation rules too
	if(o["username"].toString().isEmpty())
		return false;

	// Flags (if any) should be an array
	if(!(o["flags"].isUndefined() || o["flags"].isArray()))
		return false;

	// Group must be what we specified
	if(groupId.isEmpty()) {
		if(!(o["group"].isUndefined() || o["group"].isNull()))
			return false;
	} else {
		if(o["group"].toString() != groupId)
			return false;
	}

	// Looks like this token is valid
	return true;
}

quint64 AuthToken::generateNonce()
{
	if(sodium_init() < 0) {
		qCritical("Libsodium couldn't be initialized!");
		return 0;
	}
	quint64 nonce;
	randombytes_buf(&nonce, sizeof(nonce));
	return nonce;
}

}

