// SPDX-License-Identifier: GPL-3.0-or-later

#include "libshared/listings/zeroconfannouncement.h"
#include "libshared/net/protover.h"

#ifdef HAVE_DNSSD_BEFORE_5_84_0
#	include <KDNSSD/DNSSD/PublicService>
#else
#	include <KDNSSD/PublicService>
#endif
#include <QDateTime>

ZeroConfAnnouncement::ZeroConfAnnouncement(QObject *parent)
	: QObject(parent)
{
	m_textData["protocol"] = protocol::ProtocolVersion::current().asString().toUtf8();
}

void ZeroConfAnnouncement::publish(quint16 port)
{
	delete m_service;
	m_service = new KDNSSD::PublicService(
		QString(),
		"_drawpile._tcp",
		port,
		"local"
	);

	m_service->setParent(this);

	m_textData["started"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toUtf8();
	m_service->setTextData(m_textData);
	m_service->publishAsync();
}

void ZeroConfAnnouncement::stop()
{
	delete m_service;
	m_service = nullptr;
}

void ZeroConfAnnouncement::setTitle(const QString &title)
{
	const QByteArray t = title.toUtf8();
	if(t != m_textData["title"]) {
		m_textData["title"] = title.toUtf8();
		if(m_service)
			m_service->setTextData(m_textData);
	}
}

