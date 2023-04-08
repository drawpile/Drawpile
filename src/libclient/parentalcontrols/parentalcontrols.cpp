// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/parentalcontrols/parentalcontrols.h"
#include "libshared/util/qtcompat.h"

#include <QSettings>
#include <QRegularExpression>
#include <QVector>

namespace parentalcontrols {

Level level()
{
	int l = qBound(0, QSettings().value("pc/level", 0).toInt(), int(Level::Restricted));
	if(isOSActive())
		l = qMax(int(Level::NoJoin), l);
	return Level(l);
}

bool isLocked()
{
	return isOSActive() || !QSettings().value("pc/locked").toByteArray().isEmpty();
}

bool isLayerUncensoringBlocked()
{
	return isOSActive() || QSettings().value("pc/noUncensoring").toBool();
}

QString defaultWordList()
{
	return QStringLiteral("NSFM, NSFW, R18, R-18, K18, 18+");
}

bool isNsfmTitle(const QString &title)
{
	QSettings cfg;
	cfg.beginGroup("pc");
	if(!cfg.value("autotag", true).toBool())
		return false;

	QString wordlist = cfg.value("tagwords").toString();
	const auto words = compat::StringView{wordlist}.split(QRegularExpression("[\\s,]"), compat::SkipEmptyParts);

	for(const auto word : words) {
		if(title.contains(word, Qt::CaseInsensitive))
			return true;
	}
	return false;
}

}

