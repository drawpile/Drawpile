// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PROTOCOL_VERSION_H
#define PROTOCOL_VERSION_H

#include <QString>

namespace protocol {

/**
 * @brief A class to represent protocol versions
 */
class ProtocolVersion {
public:
	//! Construct an invalid ProtocolVersion
	ProtocolVersion() : m_namespace(QString()), m_server(-1), m_major(-1), m_minor(-1) { }

	//! Construct a ProtocolVersion
	ProtocolVersion(const QString &ns, int server, int major, int minor)
		: m_namespace(ns), m_server(server), m_major(major), m_minor(minor)
	{ }

	//! Construct a ProtocolVersion with the namespace and server version set to current values
	ProtocolVersion(int major, int minor);

	//! Get the protocol version of this build
	static ProtocolVersion current();

	/**
	 * @brief Parse a string representation of a protocl version number.
	 *
	 * String should be in the same format as produced by asString().
	 *
	 * @param str
	 * @return invalid ProtocolVersion if string was unparseable
	 */
	static ProtocolVersion fromString(const QString &str);

	/**
	 * @brief Get a string representation of this version number.
	 *
	 * Format is "ns:server.major.minor"
	 */
	QString asString() const;

	//! Is this a valid version number?
	bool isValid() const;

	//! Is this the current protocol version?
	bool isCurrent() const;

	/**
	 * Is this a newer version than the current one?
	 *
	 * Note: returns false if the namespace is different
	 */
	bool isFuture() const;

	/**
	 * Get the client version series that support this
	 * protocol version, if known. The returned string is
	 * human readable and can be shown in a UI.
	 *
	 * Returns things like "2.0.x"
	 * If the version is unknown, an empty string is returned.
	 */
	QString versionName() const;

	/**
	 * @brief Get the protocol namespace.
	 *
	 * Namespacing allows the server to host other client types than Drawpile.
	 */
	QString ns() const { return m_namespace; }

	/**
	 * @brief Get the server protocol version
	 *
	 * The server protocol number is incremented when client/server compatibility breaks.
	 * (i.e. changes are made to Transparent messages)
	 */
	int serverVersion() const { return m_server; }

	/**
	 * @brief Get the (client protocol) major version
	 *
	 * Major version is incremented when changes to clientside of the protocol are made.
	 * (i.e. changes are made to Opaque messages)
	 */
	int majorVersion() const { return m_major; }

	/**
	 * @brief Get the (client protocol) minor version
	 * Minor version is incremented when clientside changes are made that would cause
	 * incompatabilities between clients, but the structure of the messages themselves
	 * haven't changed. (E.g. brush rendering algorithm has changed, producing slightly
	 * different results for the same settings.)
	 */
	int minorVersion() const { return m_minor; }

	bool operator==(const ProtocolVersion &other) const {
			return m_namespace==other.m_namespace && m_server==other.m_server &&
					m_major==other.m_major && m_minor==other.m_minor;
	}
	bool operator!=(const ProtocolVersion &other) const {
			return !(*this==other);
	}

	/**
	 * Return the version number (without the namespace) encoded as
	 * a single integer, useful as a sorting key.
	 */
	quint64 asInteger() const;

private:
	QString m_namespace;
	int m_server;
	int m_major;
	int m_minor;
};

}

#endif
