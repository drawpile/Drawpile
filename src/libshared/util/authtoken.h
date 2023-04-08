// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AUTHTOKEN_H
#define AUTHTOKEN_H

#include <QByteArray>

class QJsonObject;

namespace server {

/**
 * @brief External signed authentication token verifier
 *
 * The auth token format is inspired by Json Web Tokens.
 *
 * The v1 format is: 1.<payload>.<signature>
 *
 * The v2 format is: 2.<payload>.<avatar>.<signature>
 *
 * Version is the token format version number. Current versions are 1 and 2.
 * Version 2 is used when the avatar image is present.
 *
 * Payload is a base64 encoded JSON object.
 * The avatar part, if present, is a base64 encoded image file.
 *
 * Signature is a base64 encoded Ed25519 signature of the everything up to (but not including) the final dot.
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
	 * @brief Get the avatar
	 *
	 * A blank bytearray is returned if no avatar was provided.
	 */
	QByteArray avatar() const { return QByteArray::fromBase64(m_avatar); }

	/**
	 * @brief Generate a random number used to identify a login request
	 * @return a random 64 bit integer
	 */
	static quint64 generateNonce();

private:
	int m_version;
	QByteArray m_payload;
	QByteArray m_avatar;
	QByteArray m_signature;
};

}

#endif

