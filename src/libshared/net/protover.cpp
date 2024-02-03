// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpmsg/protover.h>
}
#include "libshared/net/protover.h"

namespace protocol {

ProtocolVersion::ProtocolVersion()
	: ProtocolVersion(nullptr)
{
}

ProtocolVersion::ProtocolVersion(
	const QString &ns, int server, int major, int minor)
	: ProtocolVersion(
		  DP_protocol_version_new(qUtf8Printable(ns), server, major, minor))
{
}

ProtocolVersion::ProtocolVersion(int major, int minor)
	: ProtocolVersion(DP_protocol_version_new_major_minor(major, minor))
{
}

ProtocolVersion::ProtocolVersion(const ProtocolVersion &other)
	: ProtocolVersion(DP_protocol_version_new_clone(other.m_protocolVersion))
{
}

ProtocolVersion::ProtocolVersion(ProtocolVersion &&other)
	: ProtocolVersion(other.m_protocolVersion)
{
	other.m_protocolVersion = nullptr;
}

ProtocolVersion &ProtocolVersion::operator=(const ProtocolVersion &other)
{
	if(this != &other) {
		m_protocolVersion =
			DP_protocol_version_new_clone(other.m_protocolVersion);
	}
	return *this;
}

ProtocolVersion &ProtocolVersion::operator=(ProtocolVersion &&other)
{
	if(this != &other) {
		DP_protocol_version_free(m_protocolVersion);
		m_protocolVersion = other.m_protocolVersion;
		other.m_protocolVersion = nullptr;
	}
	return *this;
}

ProtocolVersion::~ProtocolVersion()
{
	DP_protocol_version_free(m_protocolVersion);
}

ProtocolVersion ProtocolVersion::current()
{
	return ProtocolVersion(DP_protocol_version_new_current());
}

ProtocolVersion ProtocolVersion::fromString(const QString &str)
{
	return ProtocolVersion(DP_protocol_version_parse(qUtf8Printable(str)));
}

QString ProtocolVersion::asString() const
{
	return QString("%1:%2.%3.%4")
		.arg(ns())
		.arg(serverVersion())
		.arg(majorVersion())
		.arg(minorVersion());
}

bool ProtocolVersion::isValid() const
{
	return DP_protocol_version_valid(m_protocolVersion);
}

bool ProtocolVersion::isCurrent() const
{
	return DP_protocol_version_is_current(m_protocolVersion);
}

bool ProtocolVersion::isFuture() const
{
	return DP_protocol_version_is_future(m_protocolVersion);
}

bool ProtocolVersion::isPastCompatible() const
{
	return DP_protocol_version_is_past_compatible(m_protocolVersion);
}

bool ProtocolVersion::shouldHaveSystemId() const
{
	return DP_protocol_version_should_have_system_id(m_protocolVersion);
}

QString ProtocolVersion::versionName() const
{
	const char *name = DP_protocol_version_name(m_protocolVersion);
	return name ? QString::fromUtf8(name) : QString();
}

QString ProtocolVersion::ns() const
{
	const char *ns = DP_protocol_version_ns(m_protocolVersion);
	return ns ? QString::fromUtf8(ns) : QString();
}

int ProtocolVersion::serverVersion() const
{
	return DP_protocol_version_server(m_protocolVersion);
}

int ProtocolVersion::majorVersion() const
{
	return DP_protocol_version_major(m_protocolVersion);
}

int ProtocolVersion::minorVersion() const
{
	return DP_protocol_version_minor(m_protocolVersion);
}

bool ProtocolVersion::operator==(const ProtocolVersion &other) const
{
	return DP_protocol_version_equals(
		m_protocolVersion, other.m_protocolVersion);
}

bool ProtocolVersion::isGreaterOrEqual(const ProtocolVersion &other) const
{
	return DP_protocol_version_greater_or_equal(
		m_protocolVersion, other.m_protocolVersion);
}

quint64 ProtocolVersion::asInteger() const
{
	return DP_protocol_version_as_integer(m_protocolVersion);
}

ProtocolVersion::ProtocolVersion(DP_ProtocolVersion *protocolVersion)
	: m_protocolVersion(protocolVersion)
{
}


}
