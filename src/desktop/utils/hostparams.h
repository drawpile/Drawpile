// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_UTILS_HOSTPARAMS_H
#define DESKTOP_UTILS_HOSTPARAMS_H
#include <QHash>
#include <QJsonArray>
#include <QMetaType>
#include <QString>
#include <QStringList>

struct HostParams {
	QString title;
	QString password;
	QString alias;
	QString address;
	QString operatorPassword;
	QStringList announcementUrls;
	bool rememberAddress;
	bool nsfm;
	bool keepChat;
	bool deputies;
	int undoLimit;
	QHash<int, int> featurePermissions;
	QJsonArray auth;
	QStringList bans;
};

Q_DECLARE_METATYPE(HostParams)

#endif
