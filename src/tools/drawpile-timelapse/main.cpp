// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <dpcommon/platform_qt.h>

extern "C" int drawpile_timelapse_main(const char *default_logo_path);

static QString writeDefaultLogo()
{
	QString appDataPath =
		QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	if(!QDir{}.mkpath(appDataPath)) {
		qWarning("Error creating directory '%s'", qUtf8Printable(appDataPath));
		return QString{};
	}

	QString path = QDir{appDataPath}.absoluteFilePath("logo1.png");
	QSaveFile out{path};
	if(!QFile::exists(path)) {
		QFile in{":/timelapse/logo1.png"};
		if(!in.open(QIODevice::ReadOnly)) {
			qWarning(
				"Error opening '%s': %s", qUtf8Printable(in.fileName()),
				qUtf8Printable(in.errorString()));
			return QString{};
		}
		QByteArray bytes = in.readAll();
		in.close();

		if(!out.open(DP_QT_WRITE_FLAGS)) {
			qWarning(
				"Error opening '%s': %s", qUtf8Printable(out.fileName()),
				qUtf8Printable(out.errorString()));
			return QString{};
		}
		out.write(bytes);
		if(!out.commit()) {
			qWarning(
				"Error writing '%s': %s", qUtf8Printable(out.fileName()),
				qUtf8Printable(out.errorString()));
			return QString{};
		}
	}

	return path;
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	DP_QT_LOCALE_RESET();
	app.setOrganizationName("drawpile");
	app.setOrganizationDomain("drawpile.net");
	app.setApplicationName("drawpile-timelapse");
	return drawpile_timelapse_main(qUtf8Printable(writeDefaultLogo()));
}
