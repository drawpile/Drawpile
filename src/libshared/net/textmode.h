/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017-2019 Calle Laakkonen

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
#ifndef DP_NET_TEXTMODE_H
#define DP_NET_TEXTMODE_H

#include "libshared/net/message.h"

namespace protocol {
namespace text {

/**
 * Text mode file parser
 */
class Parser {
public:
	struct Result {
		enum {
			Ok,
			Skip,
			Error,
			NeedMore
		} status;
		NullableMessageRef msg;
	};

	Result parseLine(const QString &line);
	QString errorString() const { return m_error; }

	Kwargs metadata() const { return m_metadata; }

	Parser() : m_state(ExpectCommand), m_ctx(0) { }

private:
	enum {
		ExpectCommand,
		ExpectKwargLine,
		ExpectDab,
	} m_state;

	Kwargs m_metadata;
	QString m_error;
	QString m_cmd;
	Kwargs m_kwargs;
	QStringList m_dabs;
	int m_ctx;
};

// Formatting helper functions
inline QString idString(uint16_t id) { return QStringLiteral("0x%1").arg(id, 4, 16, QLatin1Char('0')); }
QString idListString(const QList<uint8_t> ids);
QString idListString(const QList<uint16_t> ids);
QString rgbString(quint32 color);
QString argbString(quint32 color);
inline QString decimal(uint8_t value) { return QString::number(value/255.0*100.0, 'f', 2); }

// Parsing helper functions
uint16_t parseIdString16(const QString &id, bool *ok=nullptr);
QList<uint8_t> parseIdListString8(const QString &ids);
QList<uint16_t> parseIdListString16(const QString &ids);
quint32 parseColor(const QString &color);
inline uint8_t parseDecimal8(const QString &str) { return qBound(0, qRound(str.toFloat() / 100.0 * 255), 255); }

}
}

#endif

