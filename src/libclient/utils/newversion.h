// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DP_CLIENT_NEWVERSION_CHECK_H
#define DP_CLIENT_NEWVERSION_CHECK_H

#include <QObject>
#include <QUrl>
#include <QVector>

namespace libclient {
namespace settings {
class Settings;
}
}

class QXmlStreamReader;

/**
 * A tool for checking if a new version of the software is available.
 *
 * This class fetches and parses an AppStream appdata file and returns a
 * list of versions newer than the current one.
 *
 */
class NewVersionCheck final : public QObject {
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

		//! If this is a stable version or some kind of beta
		bool stable;
	};

	//! Construct a version checker with the current version info
	explicit NewVersionCheck(QObject *parent = nullptr);

	//! Construct a version checker with the given version
	NewVersionCheck(
		int server, int major, int minor, int beta, QObject *parent = nullptr);

	/**
	 * Is a new version check needed?
	 *
	 * A version check should be performed if:
	 *
	 *  - the user hasn't opted out
	 *  - it has been at least one day since the previous check
	 *  - if the last version checked failed, it has been at least one week
	 */
	static bool needCheck(const libclient::settings::Settings &settings);

	/**
	 * @brief Is a new series (e.g. 2.0 --> 2.1) out? (cached)
	 *
	 * Check the cached latest release if a newer incompatible version
	 * is out.
	 */
	static bool
	isThereANewSeries(const libclient::settings::Settings &settings);

	/**
	 * Explicitly set this system's platform
	 *
	 * This is used to select which artifact to include in the Version
	 * structure. If not set explicitly, the platform is selected by the build
	 * type:
	 *
	 *  - win64 if built for 64 bit windows
	 *  - win32 if built for 32 bit windows
	 *  - macos if built of macOS
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
	void
	queryVersions(libclient::settings::Settings &settings, QUrl url = QUrl());

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
	void
	versionChecked(bool isNewVersionAvailable, const QString &errorMessage);

private:
	bool parseDesktopElement(QXmlStreamReader &reader);
	bool parseReleasesElement(QXmlStreamReader &reader);

	bool isVersionNewer(int server, int major, int minor, int beta);

	void queryFail(
		libclient::settings::Settings &settings, const QString &errorMessage);
	void querySuccess(libclient::settings::Settings &settings);

	int m_server, m_major, m_minor, m_beta;
	QVector<Version> m_newer;
	QString m_platform;
};

#endif
