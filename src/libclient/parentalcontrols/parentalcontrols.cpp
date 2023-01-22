/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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

#include "parentalcontrols.h"
#include "../../libshared/util/qtcompat.h"

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

