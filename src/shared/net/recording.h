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
#ifndef DP_NET_RECORDING_H
#define DP_NET_RECORDING_H

#include <QByteArray>
#include <QString>

#include "message.h"

namespace protocol {

/**
 * @brief Event interval record
 *
 * This is used to preserve timing information in session recordings.
 *
 * Note. The maximum interval (using a single message) is about 65 seconds.
 * Typically the intervals we want to store are a few seconds at most, so this should be enough.
 */
class Interval : public Message
{
public:
	Interval(uint16_t milliseconds) : Message(MSG_INTERVAL, 0), _msecs(milliseconds) {}

	static Interval *deserialize(const uchar *data, uint len);

	uint16_t milliseconds() const { return _msecs; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint16_t _msecs;
};

/**
 * @brief A bookmark marker
 *
 * This is used to bookmark points in the session for quick access when playing back a recording.
 */
class Marker : public Message
{
public:
	Marker(uint8_t ctx, const QString &text) : Message(MSG_MARKER, ctx), _text(text.toUtf8()) { }

	static Marker *deserialize(const uchar *data, uint len);

	QString text() const { return QString::fromUtf8(_text); }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	QByteArray _text;
};

}

#endif
