// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_STARTDIALOG_HOST_FILES_H
#define DESKTOP_DIALOGS_STARTDIALOG_HOST_FILES_H
#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QJsonArray;

namespace dialogs {
namespace startdialog {
namespace host {

class SessionSettingsImporter : public QObject {
	Q_OBJECT
public:
	explicit SessionSettingsImporter(QObject *parent = nullptr);

	void importSessionSettings(const QString &path, const QByteArray &bytes);

signals:
	void importFinished(const QJsonObject &json);
	void importFailed(const QString &error);

private:
	bool readSettings(const QByteArray &bytes, QJsonObject &outSettings);
	bool readAuth(const QByteArray &bytes, QJsonArray &outAuth);
	void readBans(const QString &path, const QString &bans, bool encrypted);
	static QString getBansName(const QString &path);
};

bool exportSessionSettings(QWidget *parent, const QJsonObject &settings, QString *outError);

}
}
}

#endif
