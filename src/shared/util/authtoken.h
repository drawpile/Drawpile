/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

#ifndef AUTHTOKEN_H
#define AUTHTOKEN_H

#include <QByteArray>

class QJsonObject;

namespace server {

/**
 * @brief External signed authentication token verifier
 *
 * The auth token format is based on Json Web Tokens.
 * The format is: <version>.<payload>.<signature>.
 *
 * Version is the token format version number. Current version is 1.
 * Payload is a base64 encoded JSON object.
 * Signature is a base64 encoded Ed25519 signature of the version and payload
 */
class AuthToken {
public:
	AuthToken();
	explicit AuthToken(const QByteArray &data);

	/**
	 * @brief Is this a syntactically valid auth token?
	 *
	 * Note: this doesn't guarantee that the signature matches or
	 * that the payload contains anything sensible.
	 */
	bool isValid() const;

	/**
	 * @brief Check that the signature matches the header and payload
	 * @param pubkey the public key to check the signature against
	 * @return true if signature matches the payload and the given public key
	 */
	bool checkSignature(const QByteArray &pubkey) const;

	/**
	 * @brief Validate the payload content
	 *
	 * Note: You should call checkSignature() first to make sure
	 * the token really came from the auth server!
	 *
	 * @param groupId expected groupId (if empty, expect no group)
	 * @param nonce expected nonce
	 * @return true if content is valid
	 */
	bool validatePayload(const QString &groupId, quint64 nonce) const;

	/**
	 * @brief Get the payload
	 *
	 * Remember to check the signature first!
	 */
	QJsonObject payload() const;

	/**
	 * @brief Generate a random number used to identify a login request
	 * @return a random 64 bit integer
	 */
	static quint64 generateNonce();

private:
	int m_version;
	QByteArray m_payload;
	QByteArray m_signature;
};

}

#endif

