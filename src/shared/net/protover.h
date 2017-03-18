/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2016 Calle Laakkonen

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

private:
	QString m_namespace;
	int m_server;
	int m_major;
	int m_minor;
};

}

#endif
