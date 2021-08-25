/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2021 Calle Laakkonen

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
#ifndef DP_NET_ENVELOPE_H
#define DP_NET_ENVELOPE_H

#include <QByteArray>
#include <QtEndian>

namespace net {

/**
 * @brief A container for zero or more serialized messages
 *
 * An envelope is expected to contain whole, valid messages.
 */
class Envelope {
public:
	static const int HEADER_LEN = 4;

	Envelope()
		: m_data(QByteArray()), m_offset(0)
	{}

	Envelope(const QByteArray &b)
		: m_data(b), m_offset(0)
	{
		Q_ASSERT(b.length() >= HEADER_LEN);
	}

	Envelope(const uchar *data, int len)
		: m_data(QByteArray(reinterpret_cast<const char*>(data), len)),
		  m_offset(0)
	{
		Q_ASSERT(len >= HEADER_LEN);
	}

	//! Return the type of the first message in the envelope, or -1 if there is none
	int messageType() const {
		if(m_offset >= m_data.length())
			return -1;
		// Message format:
		// 0-1: payload length
		// 2: type
		// 3: context ID
		Q_ASSERT(length() >= HEADER_LEN);
		return sniffType(m_data.constData() + m_offset);
	}

	//! Return the length of the first message in the envelope or 0 if there is none
	int messageLength() const {
		if(isEmpty())
			return 0;
		Q_ASSERT(length() >= HEADER_LEN);
		return sniffLength(m_data.constData() + m_offset);
	}

	//! Is the first message in the envelope a Command type?
	bool isCommand() const { return messageType() >= 128; }

	//! Return an envelope starting with the next message
	Envelope next() const {
		if(m_offset >= m_data.length())
			return Envelope();

		const int next = m_offset + HEADER_LEN + qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(m_data.constData()+m_offset));
		if(next == m_data.length())
			return Envelope();

		Q_ASSERT(next+HEADER_LEN <= m_data.length());

		return Envelope(m_data, next);
	}

	//! Get the type of the message from its header
	static int sniffType(const char *data) {
		return data[2];
	}

	//! Get the length of the message from its header
	static int sniffLength(const char *data) {
		// extract payload length
		quint16 len = qFromBigEndian<quint16>((uchar*)data);

		// return total message length
		return len + HEADER_LEN;
	}

	//! Get a pointer to the beginning of the first message in the envelope
	const uchar *data() const {
		return reinterpret_cast<const uchar*>(m_data.constData() + m_offset);
	}

	//! Get the length of the envelope
	int length() const {
		return m_data.length() - m_offset;
	}

	//! Does this envelope contain no more messages?
	bool isEmpty() const {
		return m_offset >= m_data.length();
	}

	//! Append data to the envelope. The data should contain one or more full messages.
	void append(const char *data, int length)
	{
		Q_ASSERT(length >= HEADER_LEN);
		m_data.append(data, length);
	}

	//! Append the content of another envelope  to this one
	void append(const Envelope &other) {
		m_data.append(other.m_data.constData() + other.m_offset, other.length());
	}

	QPair<QByteArray, int> breakApart() const {
		return QPair<QByteArray, int>(m_data, m_offset);
	}

private:
	Envelope(const QByteArray &b, int offset)
		: m_data(b), m_offset(offset)
	{
		Q_ASSERT(b.length() - offset >= HEADER_LEN);
	}
	QByteArray m_data;
	int m_offset;
};
}

#endif
