// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/utils/newversion.h"
#include "cmake-config/config.h"
#include "libclient/settings.h"
#include "libshared/net/protover.h"
#include "libshared/util/networkaccess.h"

#include <QDate>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QXmlStreamReader>

#include <algorithm>

static const QRegularExpression
	VERSION_RE("^(\\d+)\\.(\\d+)\\.(\\d+)(?:-beta\\.(\\d+))?");

NewVersionCheck::NewVersionCheck(QObject *parent)
	: QObject(parent)
{
	QRegularExpressionMatch m = VERSION_RE.match(cmake_config::version());
	Q_ASSERT(m.hasMatch());

	m_server = m.captured(1).toInt();
	m_major = m.captured(2).toInt();
	m_minor = m.captured(3).toInt();
	m_beta = m.captured(4).toInt();

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

NewVersionCheck::NewVersionCheck(
	int server, int major, int minor, int beta, QObject *parent)
	: QObject{parent}
	, m_server{server}
	, m_major{major}
	, m_minor{minor}
	, m_beta{beta}
{
}

bool NewVersionCheck::needCheck(const libclient::settings::Settings &settings)
{
	if(!settings.versionCheckEnabled()) {
		return false;
	}

	const QDate today = QDate::currentDate();

	const QDate lastCheck =
		QDate::fromString(settings.versionCheckLastCheck(), Qt::ISODate);

	if(!lastCheck.isValid()) {
		return true;
	}

	const bool lastSuccess = settings.versionCheckLastSuccess();
	const int daysSinceLastCheck = lastCheck.daysTo(today);

	if(lastSuccess) {
		return daysSinceLastCheck > 1;
	} else {
		return daysSinceLastCheck > 7;
	}
}

bool NewVersionCheck::isThereANewSeries(
	const libclient::settings::Settings &settings)
{
	const QString latest = settings.versionCheckLatest();
	const auto m = VERSION_RE.match(latest);
	if(!m.hasMatch()) {
		return false;
	}

	const NewVersionCheck current;
	return m.captured(1).toInt() > current.m_server ||
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
			if(reader.name() == QStringLiteral("component") &&
			   reader.attributes().value("type") == QStringLiteral("desktop")) {
				if(!parseDesktopElement(reader)) {
					return false;
				}
			} else {
				qWarning(
					"Unexpected root element %s",
					qPrintable(reader.name().toString()));
				return false;
			}
			break;
		default:
			break;
		}
	}

	return true;
}

static void skipElement(QXmlStreamReader &reader)
{
	int stack = 1;
	while(!reader.atEnd() && stack > 0) {
		const auto tokentype = reader.readNext();
		switch(tokentype) {
		case QXmlStreamReader::StartElement:
			++stack;
			break;
		case QXmlStreamReader::EndElement:
			--stack;
			break;
		default:
			break;
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
				if(!parseReleasesElement(reader)) {
					return false;
				}
			} else {
				skipElement(reader);
			}
			break;
		case QXmlStreamReader::EndElement:
			return true;
		default:
			break;
		}
	}
	qWarning("Unexpected end of file while parsing <component "
			 "type=\"desktop\"> element");
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
				text << "<li>"
					 << reader.readElementText(
							QXmlStreamReader::IncludeChildElements)
					 << "</li>";

			} else {
				// skip unknown elements
				skipElement(reader);
			}
			break;

		case QXmlStreamReader::EndElement:
			return text;

		default:
			break;
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
			// See
			// https://www.freedesktop.org/software/appstream/docs/chap-Metadata.html#tag-description
			if(reader.name() == QStringLiteral("p")) {
				text << "<p>"
					 << reader.readElementText(
							QXmlStreamReader::IncludeChildElements)
					 << "</p>";

			} else if(
				reader.name() == QStringLiteral("ol") ||
				reader.name() == QStringLiteral("ul")) {
				const QString tag = reader.name().toString();
				text << "<" << tag << ">" << readList(reader) << "</" << tag
					 << ">";

			} else {
				// skip unknown elements
				skipElement(reader);
			}
			break;

		case QXmlStreamReader::EndElement:
			return text.join(QString());

		default:
			break;
		}
	}
	qWarning("Unexpected end of file while parsing <description> element");
	return QString();
}

static void parseArtifactElement(
	QXmlStreamReader &reader, NewVersionCheck::Version &release)
{
	while(!reader.atEnd()) {
		const auto tokentype = reader.readNext();

		switch(tokentype) {
		case QXmlStreamReader::StartElement:
			if(reader.name() == QStringLiteral("location")) {
				release.downloadUrl = reader.readElementText();

			} else if(reader.name() == QStringLiteral("checksum")) {
				release.downloadChecksumType =
					reader.attributes().value("type").toString();
				release.downloadChecksum = reader.readElementText();

			} else if(
				reader.name() == QStringLiteral("size") &&
				reader.attributes().value("type") ==
					QStringLiteral("download")) {
				release.downloadSize = reader.readElementText().toInt();

			} else {
				// skip other elements
				skipElement(reader);
			}
			break;

		case QXmlStreamReader::EndElement:
			return;

		default:
			break;
		}
	}
	qWarning("Unexpected end of file while parsing <release> element");
}

static void parseArtifactsElement(
	QXmlStreamReader &reader, NewVersionCheck::Version &release,
	const QString &platform)
{
	while(!reader.atEnd()) {
		const auto tokentype = reader.readNext();

		switch(tokentype) {
		case QXmlStreamReader::StartElement:
			if(reader.name() == QStringLiteral("artifact") &&
			   reader.attributes().value("type") == QStringLiteral("binary") &&
			   reader.attributes().value("platform") == platform) {
				parseArtifactElement(reader, release);

			} else {
				// skip unknown elements
				skipElement(reader);
			}
			break;

		case QXmlStreamReader::EndElement:
			return;

		default:
			break;
		}
	}
	qWarning("Unexpected end of file while parsing <artifacts> element");
}

static NewVersionCheck::Version
parseReleaseElement(QXmlStreamReader &reader, const QString &platform)
{
	NewVersionCheck::Version release;

	release.version = reader.attributes().value("version").toString();

	while(!reader.atEnd()) {
		const auto tokentype = reader.readNext();

		switch(tokentype) {
		case QXmlStreamReader::Invalid:
			return NewVersionCheck::Version{};

		case QXmlStreamReader::StartElement:
			if(reader.name() == QStringLiteral("url")) {
				release.announcementUrl = reader.readElementText();

			} else if(reader.name() == QStringLiteral("description")) {
				release.description = parseDescriptionElement(reader);

			} else if(
				reader.name() == QStringLiteral("artifacts") &&
				!platform.isEmpty()) {
				parseArtifactsElement(reader, release, platform);

			} else {
				// skip other elements
				skipElement(reader);
			}
			break;

		case QXmlStreamReader::EndElement:
			return release;

		default:
			break;
		}
	}
	qWarning("Unexpected end of file while parsing <release> element");
	return NewVersionCheck::Version{};
}

bool NewVersionCheck::parseReleasesElement(QXmlStreamReader &reader)
{
	while(!reader.atEnd()) {
		const auto tokentype = reader.readNext();

		switch(tokentype) {
		case QXmlStreamReader::Invalid:
			return false;
		case QXmlStreamReader::StartElement:
			if(reader.name() == QStringLiteral("release")) {
				const auto releaseType = reader.attributes().value("type");
				bool stable = releaseType.isEmpty() ||
							  releaseType == QStringLiteral("stable");

				if(m_beta == 0 && !stable) {
					// Skip unstable releases, unless in Beta mode
					skipElement(reader);
					break;
				}

				Version release = parseReleaseElement(reader, m_platform);
				release.stable = stable;

				if(release.version.isEmpty()) {
					qWarning("Version number missing from release element");

				} else {
					const auto m = VERSION_RE.match(release.version);

					if(!m.isValid()) {
						qWarning(
							"Invalid version string: %s",
							qPrintable(release.version));
						break;
					}

					int server = m.captured(1).toInt();
					int major = m.captured(2).toInt();
					int minor = m.captured(3).toInt();
					int beta = m.captured(4).toInt();
					if(stable && beta != 0) {
						qWarning("Purported stable release has a beta version");
						break;
					}

					if(isVersionNewer(server, major, minor, beta)) {
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
		default:
			break;
		}
	}
	qWarning("Unexpected end of file while parsing <component "
			 "type=\"desktop\"> element");
	return false;
}

bool NewVersionCheck::isVersionNewer(int server, int major, int minor, int beta)
{
	if(server > m_server) {
		return true;
	} else if(server == m_server) {
		if(major > m_major) {
			return true;
		} else if(major == m_major) {
			if(minor > m_minor) {
				return true;
			} else if(minor == m_minor) {
				if(m_beta != 0 && (beta == 0 || beta > m_beta)) {
					return true;
				}
			}
		}
	}
	return false;
}

void NewVersionCheck::queryVersions(
	libclient::settings::Settings &settings, QUrl url)
{
	if(url.isEmpty()) {
		url = QUrl("https://drawpile.net/files/metadata/net.drawpile.drawpile.appdata.xml");
	}

	qInfo("Querying %s for latest version list...", qPrintable(url.toString()));

	QNetworkRequest req(url);
	auto reply = networkaccess::getInstance()->get(req);

	connect(reply, &QNetworkReply::finished, this, [this, reply, &settings]() {
		if(reply->error() != QNetworkReply::NoError) {
			qWarning(
				"NewVersionCheck error: %s", qPrintable(reply->errorString()));
			queryFail(settings, reply->errorString());
			return;
		}

		QXmlStreamReader reader(reply);

		if(parseAppDataFile(reader)) {
			querySuccess(settings);
		} else {
			queryFail(settings, "Failed to parse version list");
		}
	});
	// Handle deletion in a separate connection so the reply object
	// won't get orphaned if this object is deleted before it finishes
	connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

void NewVersionCheck::queryFail(
	libclient::settings::Settings &settings, const QString &errorMessage)
{
	settings.setVersionCheckLastCheck(
		QDate::currentDate().toString(Qt::ISODate));
	settings.setVersionCheckLastSuccess(false);

	emit versionChecked(false, errorMessage);
}

void NewVersionCheck::querySuccess(libclient::settings::Settings &settings)
{
	settings.setVersionCheckLastCheck(
		QDate::currentDate().toString(Qt::ISODate));
	settings.setVersionCheckLastSuccess(true);

	if(m_newer.isEmpty()) {
		emit versionChecked(false, QString());
		return;
	}

	const Version latest = m_newer.first();

	const QString prevLatest = settings.versionCheckLatest();
	if(latest.version != prevLatest) {
		settings.setVersionCheckLatest(latest.version);
		emit versionChecked(true, QString());

	} else {
		emit versionChecked(false, QString());
	}
}
