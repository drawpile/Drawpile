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
#include "libshared/util/qtcompat.h"
#include "libshared/net/protover.h"
#include "config.h"

#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSettings>
#include <QDate>

#include <algorithm>

// Since any parsing failure here means that automatic updates are broken
// forever, this regular expression is as lax as possible
static const QRegularExpression VERSION_RE("(\\d+)(?:\\.(\\d+))?(?:\\.(\\d+))?[^-]*(?:-([A-Za-z0-9.-]*))?");

NewVersionCheck::NewVersionCheck(QObject *parent)
	: QObject(parent),
	m_showBetas(false)
{
	auto m = VERSION_RE.match(DRAWPILE_VERSION);
	Q_ASSERT(m.hasMatch());

	m_server = m.captured(1).toInt();
	m_major = m.captured(2).toInt();
	m_minor = m.captured(3).toInt();
	m_tag = m.captured(4);

#if defined(Q_OS_WIN64)
	m_platform = "x86_64-windows";
#elif defined(Q_OS_WIN32)
	m_platform = "i386-windows";
#elif defined(Q_OS_MACOS)
	m_platform = "darwin";
#elif defined(Q_OS_LINUX)
	m_platform = "linux";
#else
	m_platform = QString();
#endif
}

NewVersionCheck::NewVersionCheck(int server, int major, int minor, QString tag, QObject *parent)
	: QObject(parent),
	m_server(server),
	m_major(major),
	m_minor(minor),
	m_tag(tag),
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

	const QDate lastSuccess = QDate::fromString(cfg.value("lastsuccess").toString(), Qt::ISODate);

	if(!lastSuccess.isValid())
		return true;

	const int daysSinceLastCheck = lastCheck.daysTo(today);
	if(lastSuccess == lastCheck)
		return daysSinceLastCheck > 1;
	else {
		// We want to know how much time has passed between failure and last
		// check, not whether someone just did not open Drawpile for a month,
		// so do not use `today`
		const int daysSinceLastSuccess = lastSuccess.daysTo(lastCheck);

		// Forcibly disable version checks if they keep failing so the domain is
		// not flooded with traffic from old clients forever
		if(daysSinceLastSuccess > 30) {
			cfg.setValue("enabled", false);
			return false;
		}

		return daysSinceLastCheck > std::min(7, daysSinceLastSuccess * 2);
	}
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
			if(reader.name() == QStringLiteral("artifact") && reader.attributes().value("type") == QStringLiteral("binary") && reader.attributes().value("platform").contains(platform)) {
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

// Precondition: Parts before the tag should have been equal
static bool isNewerTag(compat::StringView current, compat::StringView other) {
	// When major, minor, and patch are equal, a pre-release version has lower
	// precedence than a normal version
	if(current.isEmpty()) {
		// Current is not a pre-release, so newer
		return false;
	} else if(other.isEmpty()) {
		// Other is not a pre-release, so newer
		return true;
	}

	// Precedence for two pre-release versions with the same major, minor, and
	// patch version MUST be determined by comparing each dot separated
	// identifier from left to right until a difference is found
	const auto current_parts = current.split('.');
	const auto other_parts = other.split('.');
	for(auto i = 0; i < std::min(current_parts.size(), other_parts.size()); ++i) {
		const auto &current_part = current_parts[i];
		const auto &other_part = other_parts[i];

		if(current_part == other_part) {
			continue;
		} else {
			bool current_is_numeric{};
			bool other_is_numeric{};
			const auto current_n = current_part.toUInt(&current_is_numeric);
			const auto other_n = other_part.toUInt(&other_is_numeric);

			if(current_is_numeric && other_is_numeric) {
				// Identifiers consisting of only digits are compared
				// numerically.
				return current_n < other_n;
			} else if(current_is_numeric || other_is_numeric) {
				// Numeric identifiers always have lower precedence than
				// non-numeric identifiers.
				return current_is_numeric;
			}

			// Identifiers with letters or hyphens are compared lexically in
			// ASCII sort order.
			return current_part < other_part;
		}
	}

	// A larger set of pre-release fields has a higher precedence than a smaller
	// set, if all of the preceding identifiers are equal.
	return current_parts.size() < other_parts.size();
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
					else if(versionNumber == currentVersionNumber && isNewerTag(m_tag, m.captured(4))) {
						m_newer << release;
					}
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

			// Respect a 410 status as an explicit signal to kill auto-update
			if(reply->error() == QNetworkReply::ContentGoneError) {
				QSettings().setValue("versioncheck/enabled", false);
			}

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

	emit versionChecked(false, errorMessage);
}

void NewVersionCheck::querySuccess()
{
	QSettings cfg;
	const auto now = QDate::currentDate().toString(Qt::ISODate);
	cfg.beginGroup("versioncheck");
	cfg.setValue("lastcheck", now);
	cfg.setValue("lastsuccess", now);

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

