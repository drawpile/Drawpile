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

#ifndef DP_CLIENT_NEWVERSION_CHECK_H
#define DP_CLIENT_NEWVERSION_CHECK_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>

class QXmlStreamReader;

/**
 * A tool for checking if a new version of the software is available.
 *
 * This class fetches and parses an AppStream appdata file and returns a
 * list of versions newer than the current one.
 *
 */
class NewVersionCheck : public QObject {
	Q_OBJECT
public:
	//! Discovered version
	struct Version {
		//! Version string
		QString version;

		//! URL of the release announcement page
		QString announcementUrl;

		//! What's new in this version (in a simple HTML subset format)
		QString description;

		//! Direct download link
		QString downloadUrl;

		//! Hash of the download
		QString downloadChecksum;

		//! Type of the checksum (e.g. sha256)
		QString downloadChecksumType;

		//! Size of the download in bytes
		int downloadSize = 0;
	};

	//! Construct a version checker with the current version info
	explicit NewVersionCheck(QObject *parent=nullptr);

	//! Construct a version checker with the given version
	NewVersionCheck(int server, int major, int minor, QString tag=QString(), QObject *parent=nullptr);

	/**
	 * Is a new version check needed?
	 *
	 * A version check should be performed if:
	 * 
	 *  - the user hasn't opted out (QSettings: versioncheck/enabled, default=true)
	 *  - it has been at least one day since the previous check
	 *  - if the last version checked failed, it has been at least one week
	 */
	static bool needCheck();

	/**
	 * @brief Is a new series (e.g. 2.0 --> 2.1) out? (cached)
	 *
	 * Check the cached latest release if a newer incompatible version
	 * is out.
	 */
	static bool isThereANewSeries();

	/**
	 * Show beta releases?
	 *
	 * By default, only stable releases are shown.
	 * This must be called before queryVersion or parseAppDataFile.
	 */
	void setShowBetas(bool show) { m_showBetas = show; }

	/**
	 * Explicitly set this system's platform
	 *
	 * This is used to select which artifact to include in the Version structure.
	 * If not set explicitly, the platform is selected by the build type:
	 *
	 *  - x86_64-windows if built for 64 bit windows
	 *  - i386-windows if built for 32 bit windows
	 *  - darwin if built for macOS
	 *  - linux if built for Linux
	 *  - blank for everything else
	 *
	 * If a blank platform is set, the download fields will not be populated.
	 */
	void setPlatform(const QString &platform) { m_platform = platform; }

	/**
	 * Make a HTTP request and check the version file
	 *
	 * Emits versionChecked when done.
	 * The "newVersionAvailable" parameter will be true if a new version has
	 * been released since the last time queryVersions was called.
	 *
	 * @param url the URL to query (if null, the default built-in URL is used)
	 */
	void queryVersions(QUrl url=QUrl());

	/**
	 * Parse an AppData file and get the list of releases.
	 *
	 * Typically, you should not need to call this directly. Call
	 * queryVersions() instead to fetch and parse the version list.
	 *
	 * @return true if the file was parsed successfully
	 */
	bool parseAppDataFile(QXmlStreamReader &reader);

	/**
	 * Get all available versions newer than this one.
	 *
	 * The list of new versions is populated by parseAppDataFile,
	 * which (or queryVersions, rather) should be called before this.
	 * The list is returned in the order it appears in the appdata file.
	 *
	 * If nothing newer is available, an empty vector is returned.
	 */
	QVector<Version> getNewer() const { return m_newer; }

signals:
	/**
	 * @brief Version check complete
	 *
	 * The errorMessage parameter will be non-empty if an error occurred.
	 */
	void versionChecked(bool isNewVersionAvailable, const QString &errorMessage);

private:
	bool parseDesktopElement(QXmlStreamReader &reader);
	bool parseReleasesElement(QXmlStreamReader &reader);

	void queryFail(const QString &errorMessage);
	void querySuccess();

	int m_server, m_major, m_minor;
	QString m_tag;
	QVector<Version> m_newer;
	bool m_showBetas;
	QString m_platform;
};

#endif

