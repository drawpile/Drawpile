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

#include "protover.h"
#include "../../config.h"

#include <QRegularExpression>

namespace protocol {

ProtocolVersion::ProtocolVersion(int major, int minor)
	: m_namespace(QStringLiteral("dp")),
	  m_server(DRAWPILE_PROTO_SERVER_VERSION),
	  m_major(major),
	  m_minor(minor)
{
}

bool ProtocolVersion::isValid() const
{
	if(m_namespace.isEmpty() || m_server<0 || m_major<0 || m_minor<0)
		return false;

	for(int i=0;i<m_namespace.length();++i)
		if(m_namespace.at(i) < 'a' || m_namespace.at(i) > 'z')
			return false;

	return true;
}

ProtocolVersion ProtocolVersion::current()
{
	return ProtocolVersion(
			QStringLiteral("dp"),
			DRAWPILE_PROTO_SERVER_VERSION,
			DRAWPILE_PROTO_MAJOR_VERSION,
			DRAWPILE_PROTO_MINOR_VERSION
			);
}

bool ProtocolVersion::isCurrent() const
{
	return m_namespace == QStringLiteral("dp") &&
			m_server == DRAWPILE_PROTO_SERVER_VERSION &&
			m_major == DRAWPILE_PROTO_MAJOR_VERSION &&
			m_minor == DRAWPILE_PROTO_MINOR_VERSION;
}

ProtocolVersion ProtocolVersion::fromString(const QString &str)
{
	QRegularExpression re("([a-z]+):(\\d+)\\.(\\d+)\\.(\\d+)");
	auto m = re.match(str);
	if(!m.isValid())
		return ProtocolVersion();

	return ProtocolVersion(
				m.captured(1),
				m.captured(2).toInt(),
				m.captured(3).toInt(),
				m.captured(4).toInt()
				);
}

QString ProtocolVersion::asString() const
{
	return QString("%1:%2.%3.%4").arg(m_namespace).arg(m_server).arg(m_major).arg(m_minor);
}


}
