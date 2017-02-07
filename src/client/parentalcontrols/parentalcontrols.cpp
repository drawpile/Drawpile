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

#include <QSettings>
#include <QRegularExpression>
#include <QVector>

namespace parentalcontrols {

Level level()
{
	int l = qBound(0, QSettings().value("pc/level", 1).toInt(), int(Level::Restricted));
	if(isOSActive())
		l = qMax(int(Level::NoJoin), l);
	return Level(l);
}

bool isLocked()
{
	return isOSActive() || !QSettings().value("pc/locked").toByteArray().isEmpty();
}

QString defaultWordList()
{
	return QStringLiteral("NSFM, NSFW, R18, R-18, K18, 18+");
}

bool isNsfmTitle(const QString &title)
{
	QString wordlist = QSettings().value("pc/tagwords").toString();
	for(const QStringRef &word : wordlist.splitRef(QRegularExpression("[\\s,]"), QString::SkipEmptyParts)) {
		if(title.contains(word, Qt::CaseInsensitive))
			return true;
	}
	return false;
}

}

