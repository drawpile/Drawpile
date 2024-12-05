// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/host/files.h"
#include "desktop/dialogs/startdialog/host/categories.h"
#include "desktop/filewrangler.h"
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

namespace dialogs {
namespace startdialog {
namespace host {

namespace {
static const QByteArray settingsPrefix("dphost1\0", 8);
static const QByteArray authPrefix("dpauth\0", 7);
static QRegularExpression
	plainBansRe(QStringLiteral("\\Ap([a-zA-Z0-9+/=]+)\\z"));
static QRegularExpression encryptedBansRe(
	QStringLiteral("\\Ac([a-zA-Z0-9+/=]+):([a-zA-Z0-9+/=]+)\\z"));
}


SessionSettingsImporter::SessionSettingsImporter(QObject *parent)
	: QObject(parent)
{
}

void SessionSettingsImporter::importSessionSettings(
	const QString &path, const QByteArray &bytes)
{
	if(bytes.startsWith(settingsPrefix)) {
		QJsonObject settings;
		if(readSettings(bytes, settings)) {
			emit importFinished(settings);
		}
	} else if(bytes.startsWith(authPrefix)) {
		QJsonArray auth;
		if(readAuth(bytes, auth)) {
			emit importFinished(QJsonObject(
				{{QStringLiteral("roles"),
				  QJsonObject({{QStringLiteral("auth"), auth}})}}));
		}
	} else {
		QString string = QString::fromUtf8(bytes);
		if(plainBansRe.match(string).hasMatch()) {
			readBans(path, string, false);
		} else if(encryptedBansRe.match(string).hasMatch()) {
			readBans(path, string, true);
		} else {
			emit importFailed(
				tr("This file doesn't contain any valid session settings."));
		}
	}
}

bool SessionSettingsImporter::readSettings(
	const QByteArray &bytes, QJsonObject &outSettings)
{
	QJsonDocument doc =
		QJsonDocument::fromJson(qUncompress(bytes.mid(settingsPrefix.size())));
	if(!doc.isObject()) {
		emit importFailed(tr("Invalid session settings format."));
		return false;
	}

	QJsonObject json = doc.object();
	for(int i = 0; i < int(Categories::Count); ++i) {
		QString key = Categories::getCategoryName(i);
		QJsonValue value = json[key];
		if(value.isObject()) {
			outSettings[key] = value;
		}
	}

	if(outSettings.isEmpty()) {
		emit importFailed(tr("File contains session settings to import."));
		return false;
	}

	return true;
}

bool SessionSettingsImporter::readAuth(
	const QByteArray &bytes, QJsonArray &outAuth)
{
	QJsonDocument doc =
		QJsonDocument::fromJson(qUncompress(bytes.mid(authPrefix.size())));
	if(!doc.isArray()) {
		emit importFailed(tr("Invalid role format."));
		return false;
	}

	for(const QJsonValue &value : doc.array()) {
		QString authId = value[QStringLiteral("a")].toString();
		if(!authId.isEmpty()) {
			QJsonObject o = {
				{QStringLiteral("a"), authId},
				{QStringLiteral("u"), value[QStringLiteral("u")].toString()},
			};
			if(value[QStringLiteral("o")].toBool()) {
				o[QStringLiteral("o")] = true;
			}
			if(value[QStringLiteral("t")].toBool()) {
				o[QStringLiteral("t")] = true;
			}
			outAuth.append(o);
		}
	}

	if(outAuth.isEmpty()) {
		emit importFailed(tr("File contains no roles to import."));
		return false;
	}

	return true;
}

void SessionSettingsImporter::readBans(
	const QString &path, const QString &bans, bool encrypted)
{
	QJsonObject json = {
		{QStringLiteral("bans"),
		 QJsonObject({
			 {QStringLiteral("list"),
			  QJsonArray({
				  QJsonObject({
					  {QStringLiteral("name"), getBansName(path)},
					  {QStringLiteral("data"), bans},
					  {QStringLiteral("encrypted"), encrypted},
				  }),
			  })},
		 })},
	};
	emit importFinished(json);
}

QString SessionSettingsImporter::getBansName(const QString &path)
{
	if(path.isEmpty()) {
		return QStringLiteral("bans");
	} else {
		QString baseName = QFileInfo(path).fileName();
		return baseName.isEmpty() ? path : baseName;
	}
}


bool exportSessionSettings(
	QWidget *parent, const QJsonObject &settings, QString *outError)
{
	return FileWrangler(parent).saveSessionSettings(
		settingsPrefix +
			qCompress(QJsonDocument(settings).toJson(QJsonDocument::Compact)),
		outError);
}

}
}
}
