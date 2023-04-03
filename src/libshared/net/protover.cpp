// SPDX-License-Identifier: GPL-3.0-or-later

#include "libshared/net/protover.h"
#include "cmake-config/config.h"

#include <QRegularExpression>

namespace protocol {

ProtocolVersion::ProtocolVersion(int major, int minor)
	: m_namespace(QStringLiteral("dp")),
	  m_server(cmake_config::proto::server()),
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

quint64 ProtocolVersion::asInteger() const
{
	const quint64 a = qBound(0ull, quint64(m_server), quint64((1<<21) - 1));
	const quint64 b = qBound(0ull, quint64(m_major), quint64((1<<21) - 1));
	const quint64 c = qBound(0ull, quint64(m_minor), quint64((1<<21) - 1));

	return (a<<42) | (b<<21) | c;
}

ProtocolVersion ProtocolVersion::current()
{
	return ProtocolVersion(
			QStringLiteral("dp"),
			cmake_config::proto::server(),
			cmake_config::proto::major(),
			cmake_config::proto::minor()
			);
}

bool ProtocolVersion::isCurrent() const
{
	return m_namespace == QStringLiteral("dp") &&
			m_server == cmake_config::proto::server() &&
			m_major == cmake_config::proto::major() &&
			m_minor == cmake_config::proto::minor();
}

bool ProtocolVersion::isFuture() const
{
	return
		m_namespace == QStringLiteral("dp") &&
		asInteger() > current().asInteger();
}

bool ProtocolVersion::isPastCompatible() const
{
	return m_namespace == QStringLiteral("dp") &&
			m_server == 4 && m_major == 21 && m_minor == 2;
}

QString ProtocolVersion::versionName() const
{
	if(m_namespace != QStringLiteral("dp"))
		return QString();

	if(m_server == 4 && m_major == 22)
		return QStringLiteral("2.2.0 beta");
	else if(m_server == 4 && m_major == 21 && m_minor == 2)
		return QStringLiteral("2.1.x");
	else if(m_server == 4 && m_major == 20 && m_minor == 1)
		return QStringLiteral("2.0.x");

	// Unknown (possibly future) version
	return QString();
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
