/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#include "libclient/utils/newversion.h"
#include "libshared/util/networkaccess.h"
#include "libshared/net/protover.h"
#include "config.h"

#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSettings>
#include <QDate>

#include <algorithm>

static const QRegularExpression VERSION_RE("^(\\d+)\\.(\\d+)\\.(\\d+)");

NewVersionCheck::NewVersionCheck(QObject *parent)
	: QObject(parent),
	m_showBetas(false)
{
	auto m = VERSION_RE.match(DRAWPILE_VERSION);
	Q_ASSERT(m.hasMatch());

	m_server = m.captured(1).toInt();
	m_major = m.captured(2).toInt();
	m_minor = m.captured(3).toInt();

#if defined(Q_OS_WIN64)
	m_platform = "win64";
#elif defined(Q_OS_WIN32)
	m_platform = "win32";
#elif defined(Q_OS_MACOS)
	m_platform = "macos";
#else
	m_platform = QString();
#endif
}

NewVersionCheck::NewVersionCheck(int server, int major, int minor, QObject *parent)
	: QObject(parent),
	m_server(server),
	m_major(major),
	m_minor(minor),
	m_showBetas(false)
{
}

bool NewVersionCheck::needCheck()
{
	QSettings cfg;
	cfg.beginGroup("versioncheck");
	if(!cfg.value("enabled", true).toBool())
		return false;

	const QDate today = QDate::currentDate();

	const QDate lastCheck = QDate::fromString(cfg.value("lastcheck").toString(), Qt::ISODate);

	if(!lastCheck.isValid())
		return true;

	const bool lastSuccess = cfg.value("lastsuccess").toBool();
	const int daysSinceLastCheck = lastCheck.daysTo(today);

	if(lastSuccess)
		return daysSinceLastCheck > 1;
	else
		return daysSinceLastCheck > 7;
}

bool NewVersionCheck::isThereANewSeries()
{
	const QString latest = QSettings().value("versioncheck/latest").toString();
	const auto m = VERSION_RE.match(latest);
	if(!m.hasMatch())
		return false;

	const NewVersionCheck current;
	return
		m.captured(1).toInt() > current.m_server ||
		m.captured(2).toInt() > current.m_major;
}

bool NewVersionCheck::parseAppDataFile(QXmlStreamReader &reader)
{
	m_newer.clear();

	while(!reader.atEnd()) {
		const auto tokentype = reader.readNext();

		switch(tokentype) {
		case QXmlStreamReader::Invalid:
			return false;
		case QXmlStreamReader::StartElement:
			if(reader.name() == QStringLiteral("component") && reader.attributes().value("type") == QStringLiteral("desktop")) {
				if(!parseDesktopElement(reader))
					return false;

			} else {
				qWarning("Unexpected root element %s", qPrintable(reader.name().toString()));
				return false;
			}
			break;
		default: break;
		}
	}

	return true;
}

static void skipElement(QXmlStreamReader &reader)
{
	int stack=1;
	while(!reader.atEnd() && stack > 0) {
		const auto tokentype = reader.readNext();
		switch(tokentype) {
		case QXmlStreamReader::StartElement: ++stack; break;
		case QXmlStreamReader::EndElement: --stack; break;
		default: break;
		}
	}
}

bool NewVersionCheck::parseDesktopElement(QXmlStreamReader &reader)
{
	while(!reader.atEnd()) {
		const auto tokentype = reader.readNext();

		switch(tokentype) {
		case QXmlStreamReader::Invalid:
			return false;
		case QXmlStreamReader::StartElement:
			if(reader.name() == QStringLiteral("releases")) {
				if(!parseReleasesElement(reader))
					return false;

			} else {
				skipElement(reader);
			}
			break;
		case QXmlStreamReader::EndElement:
			return true;
		default: break;
		}
	}
	qWarning("Unexpected end of file while parsing <component type=\"desktop\"> element");
	return false;
}

static QStringList readList(QXmlStreamReader &reader)
{
	QStringList text;
	while(!reader.atEnd()) {
		const auto tokentype = reader.readNext();

		switch(tokentype) {
		case QXmlStreamReader::Invalid:
			return QStringList();

		case QXmlStreamReader::StartElement:
			if(reader.name() == QStringLiteral("li")) {
				text << "<li>" << reader.readElementText(QXmlStreamReader::IncludeChildElements) << "</li>";

			} else {
				// skip unknown elements
				skipElement(reader);
			}
			break;

		case QXmlStreamReader::EndElement:
			return text;

		default: break;
		}
	}
	qWarning("Unexpected end of file while parsing list element");
	return text;
}

static QString parseDescriptionElement(QXmlStreamReader &reader)
{
	QStringList text;

	while(!reader.atEnd()) {
		const auto tokentype = reader.readNext();

		switch(tokentype) {
		case QXmlStreamReader::Invalid:
			return QString();

		case QXmlStreamReader::StartElement:
			// See https://www.freedesktop.org/software/appstream/docs/chap-Metadata.html#tag-description
			if(reader.name() == QStringLiteral("p")) {
				text << "<p>" << reader.readElementText(QXmlStreamReader::IncludeChildElements) << "</p>";

			} else if(reader.name() == QStringLiteral("ol") || reader.name() == QStringLiteral("ul")) {
				const QString tag = reader.name().toString();
				text
					<< "<" << tag << ">"
					<< readList(reader)
					<< "</" << tag << ">";

			} else {
				// skip unknown elements
				skipElement(reader);
			}
			break;

		case QXmlStreamReader::EndElement:
			return text.join(QString());

		default: break;
		}
	}
	qWarning("Unexpected end of file while parsing <description> element");
	return QString();

}

static void parseArtifactElement(QXmlStreamReader &reader, NewVersionCheck::Version &release)
{
	while(!reader.atEnd()) {
		const auto tokentype = reader.readNext();

		switch(tokentype) {
		case QXmlStreamReader::StartElement:
			if(reader.name() == QStringLiteral("location")) {
				release.downloadUrl = reader.readElementText();

			} else if(reader.name() == QStringLiteral("checksum")) {
				release.downloadChecksumType = reader.attributes().value("type").toString();
				release.downloadChecksum = reader.readElementText();

			} else if(reader.name() == QStringLiteral("size") && reader.attributes().value("type") == QStringLiteral("download")) {
				release.downloadSize = reader.readElementText().toInt();

			} else {
				// skip other elements
				skipElement(reader);
			}
			break;

		case QXmlStreamReader::EndElement:
			return;

		default: break;
		}
	}
	qWarning("Unexpected end of file while parsing <release> element");
}

static void parseArtifactsElement(QXmlStreamReader &reader, NewVersionCheck::Version &release, const QString &platform)
{
	while(!reader.atEnd()) {
		const auto tokentype = reader.readNext();

		switch(tokentype) {
		case QXmlStreamReader::StartElement:
			if(reader.name() == QStringLiteral("artifact") && reader.attributes().value("type") == QStringLiteral("binary") && reader.attributes().value("platform") == platform) {
				parseArtifactElement(reader, release);

			} else {
				// skip unknown elements
				skipElement(reader);
			}
			break;

		case QXmlStreamReader::EndElement:
			return;

		default: break;
		}
	}
	qWarning("Unexpected end of file while parsing <artifacts> element");
}

static NewVersionCheck::Version parseReleaseElement(QXmlStreamReader &reader, const QString &platform)
{
	NewVersionCheck::Version release;

	release.version = reader.attributes().value("version").toString();

	while(!reader.atEnd()) {
		const auto tokentype = reader.readNext();

		switch(tokentype) {
		case QXmlStreamReader::Invalid:
			return NewVersionCheck::Version {};

		case QXmlStreamReader::StartElement:
			if(reader.name() == QStringLiteral("url")) {
				release.announcementUrl = reader.readElementText();

			} else if(reader.name() == QStringLiteral("description")) {
				release.description = parseDescriptionElement(reader);

			} else if(reader.name() == QStringLiteral("artifacts") && !platform.isEmpty()) {
				parseArtifactsElement(reader, release, platform);

			} else {
				// skip other elements
				skipElement(reader);
			}
			break;

		case QXmlStreamReader::EndElement:
			return release;

		default: break;
		}
	}
	qWarning("Unexpected end of file while parsing <release> element");
	return NewVersionCheck::Version {};
}

bool NewVersionCheck::parseReleasesElement(QXmlStreamReader &reader)
{
	const auto currentVersionNumber = protocol::ProtocolVersion(QStringLiteral("dp"), m_server, m_major, m_minor).asInteger();

	while(!reader.atEnd()) {
		const auto tokentype = reader.readNext();

		switch(tokentype) {
		case QXmlStreamReader::Invalid:
			return false;
		case QXmlStreamReader::StartElement:
			if(reader.name() == QStringLiteral("release")) {
				const auto releaseType = reader.attributes().value("type");

				if(!m_showBetas && !releaseType.isEmpty() && releaseType != QStringLiteral("stable")) {
					// Skip unstable releases, unless in Beta mode
					skipElement(reader);
					break;
				}

				const Version release = parseReleaseElement(reader, m_platform);

				if(release.version.isEmpty()) {
					qWarning("Version number missing from release element");

				} else {
					const auto m = VERSION_RE.match(release.version);

					if(!m.isValid()) {
						qWarning("Invalid version string: %s", qPrintable(release.version));
						break;
					}

					const auto versionNumber = protocol::ProtocolVersion(
						QStringLiteral("dp"),
						m.captured(1).toInt(),
						m.captured(2).toInt(),
						m.captured(3).toInt()
					).asInteger();

					if(versionNumber > currentVersionNumber)
						m_newer << release;
				}

			} else {
				// skip unknown elements
				skipElement(reader);
			}
			break;
		case QXmlStreamReader::EndElement:
			return true;
		default: break;
		}
	}
	qWarning("Unexpected end of file while parsing <component type=\"desktop\"> element");
	return false;
}

void NewVersionCheck::queryVersions(QUrl url)
{
	if(url.isEmpty())
		url = QUrl("https://drawpile.net/files/metadata/net.drawpile.drawpile.appdata.xml");

	qInfo("Querying %s for latest version list...", qPrintable(url.toString()));

	QNetworkRequest req(url);
	auto reply = networkaccess::getInstance()->get(req);

	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		if(reply->error() != QNetworkReply::NoError) {
			qWarning("NewVersionCheck error: %s", qPrintable(reply->errorString()));
			queryFail(reply->errorString());
			return;
		}

		QXmlStreamReader reader(reply);

		if(parseAppDataFile(reader))
			querySuccess();
		else
			queryFail("Failed to parse version list");
	});
	// Handle deletion in a separate connection so the reply object
	// won't get orphaned if this object is deleted before it finishes
	connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

void NewVersionCheck::queryFail(const QString &errorMessage)
{
	QSettings cfg;
	cfg.beginGroup("versioncheck");
	cfg.setValue("lastcheck", QDate::currentDate().toString(Qt::ISODate));
	cfg.setValue("lastsuccess", false);

	emit versionChecked(false, errorMessage);
}

void NewVersionCheck::querySuccess()
{
	QSettings cfg;
	cfg.beginGroup("versioncheck");
	cfg.setValue("lastcheck", QDate::currentDate().toString(Qt::ISODate));
	cfg.setValue("lastsuccess", true);

	if(m_newer.isEmpty()) {
		emit versionChecked(false, QString());
		return;
	}

	const Version latest = m_newer.first();

	const QString prevLatest = cfg.value("latest").toString();
	if(latest.version != prevLatest) {
		cfg.setValue("latest", latest.version);
		emit versionChecked(true, QString());

	} else {
		emit versionChecked(false, QString());
	}
}

