// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/parentalcontrols/parentalcontrols.h"
#include "libclient/settings.h"
#include "libshared/util/qtcompat.h"

#include <QPointer>
#include <QRegularExpression>
#include <QVector>

namespace parentalcontrols {

QPointer<libclient::settings::Settings> g_settings;

Level level()
{
	int l = qBound(0, g_settings ? int(g_settings->parentalControlsLevel()) : 0, int(Level::Restricted));
	if(isOSActive())
		l = qMax(int(Level::NoJoin), l);
	return Level(l);
}

bool isLocked()
{
	return isOSActive() || (g_settings && !g_settings->parentalControlsLocked().isEmpty());
}

bool isLayerUncensoringBlocked()
{
	return isOSActive() || (g_settings && g_settings->parentalControlsForceCensor());
}

QString defaultWordList()
{
	return QStringLiteral("NSFM, NSFW, R18, R-18, K18, 18+");
}

bool isNsfmTitle(const QString &title)
{
	if(!g_settings || !g_settings->parentalControlsAutoTag())
		return false;

	QString wordlist = g_settings->parentalControlsTags();
	const auto words = compat::StringView{wordlist}.split(QRegularExpression("[\\s,]"), compat::SkipEmptyParts);

	for(const auto word : words) {
		if(title.contains(word, Qt::CaseInsensitive))
			return true;
	}
	return false;
}

bool isNsfmAlias(const QString &alias)
{
	return isNsfmTitle(alias);
}

}

