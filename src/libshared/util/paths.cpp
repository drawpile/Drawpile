// SPDX-License-Identifier: GPL-3.0-or-later
#include "libshared/util/paths.h"
#include "libshared/util/qtcompat.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <cmake-config/config.h>
#ifdef Q_OS_ANDROID
#	include <QRegularExpression>
#endif

namespace utils {
namespace paths {

static QStringList DATAPATHS;
static QString WRITABLEPATH;

void setDataPath(const QString &datapath)
{
	DATAPATHS = QStringList() << QDir{datapath}.absolutePath();
}

void setWritablePath(const QString &datapath)
{
	WRITABLEPATH = QDir{datapath}.absolutePath();

	dataPaths(); // used for side effect
	DATAPATHS.insert(0, WRITABLEPATH);
}

QStringList dataPaths()
{
	if(DATAPATHS.isEmpty()) {
#ifdef __EMSCRIPTEN__
		DATAPATHS << QStringLiteral("/assets");
#else
#	ifdef Q_OS_ANDROID
		DATAPATHS << QStringLiteral("assets:");
#	endif
#	ifdef DRAWPILE_SOURCE_ASSETS_DESKTOP
		DATAPATHS << QString::fromUtf8(cmake_config::sourceAssetsDesktop());
#	endif
		DATAPATHS << QStandardPaths::standardLocations(
			QStandardPaths::AppDataLocation);
#endif
	}
	return DATAPATHS;
}

QString locateDataFile(const QString &filename)
{
	for(const QString &datapath : dataPaths()) {
		const QDir d{datapath};
		if(d.exists(filename))
			return d.filePath(filename);
	}
	return QString();
}

QString writablePath(
	QStandardPaths::StandardLocation location, const QString &dirOrFileName,
	const QString &filename)
{
	QString path;
	if(WRITABLEPATH.isEmpty()) {
#ifdef __EMSCRIPTEN__
		path = "/appdata";
#else
		path = QStandardPaths::writableLocation(location);
#endif

	} else {
		switch(location) {
		case QStandardPaths::AppConfigLocation:
			path = WRITABLEPATH + QStringLiteral("/settings");
			break;
		case QStandardPaths::AppDataLocation:
		case QStandardPaths::AppLocalDataLocation:
			path = WRITABLEPATH;
			break;
		case QStandardPaths::CacheLocation:
			path = WRITABLEPATH + QStringLiteral("/cache");
			break;
		default:
			path = QStandardPaths::writableLocation(location);
		}
	}

	// Setting both dirOrFilename and filename means dirOrFilename is treated
	// as a directory name and must be created and filename is the name of the
	// actual file Both can be ".", in which case the writableLocation is
	// returned as is, but the path is still created.
	if(filename.isEmpty()) {
		if(!QDir().mkpath(path)) {
			qWarning("Error creating directory %s", qUtf8Printable(path));
		}
	} else {
		if(!QDir(path).mkpath(dirOrFileName)) {
			qWarning(
				"Error creating directory %s/%s", qUtf8Printable(path),
				qUtf8Printable(dirOrFileName));
		}
	}

	if(!dirOrFileName.isEmpty() && dirOrFileName != ".") {
		if(!path.endsWith('/'))
			path.append('/');
		path += dirOrFileName;
	}

	if(!filename.isEmpty() && filename != ".") {
		if(!path.endsWith('/'))
			path.append('/');
		path += filename;
	}

	return path;
}

QString extractBasename(QString filename)
{
	const QFileInfo file(filename);
	QString title = file.fileName();
#ifdef Q_OS_ANDROID
	// On Android, the file "name" is a URI-encoded path thing. We find the
	// last : (%3A) or / (%2F) and chop off anything that comes before that.
	// That appears to consistently give just the actual file name.
	int i = title.lastIndexOf(QRegularExpression{
		"(?<=%3a|%2f)", QRegularExpression::CaseInsensitiveOption});
	if(i != -1) {
		title.remove(0, i);
	}
#endif
	return title;
}

bool slurp(const QString &path, QByteArray &outBytes, QString &outError)
{
	QFile file(path);
	if(!file.open(QIODevice::ReadOnly)) {
		outError = file.errorString();
		return false;
	}

	qint64 size = file.size();
	if(size < 0) {
		outError = file.errorString();
		file.close();
		return false;
	}

	if(size >= std::numeric_limits<compat::sizetype>::max()) {
		file.close();
		outError = QCoreApplication::translate(
			"utils::paths", "File size out of bounds");
		return false;
	}

	outBytes.resize(size);
	qint64 read = file.read(outBytes.data(), size);
	if(read == -1) {
		outError = file.errorString();
		file.close();
		return false;
	} else if(read != size) {
		file.close();
		outError = QCoreApplication::translate(
			"utils::paths", "Could not read entire file");
		return false;
	} else {
		file.close();
		return true;
	}
}

}
}
