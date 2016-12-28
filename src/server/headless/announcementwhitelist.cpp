/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#include "announcementwhitelist.h"
#include "../shared/util/logger.h"

#include <QUrl>
#include <QFile>

AnnouncementWhitelist::AnnouncementWhitelist(QObject *parent) : QObject(parent)
{

}

bool AnnouncementWhitelist::isWhitelisted(const QUrl &url)
{
	if(_file.isModified())
		reloadWhitelist();

	QString urlstr = url.toString();

	for(const QRegularExpression &re : _whitelist) {
		if(re.match(urlstr).hasMatch())
			return true;
	}

	return false;
}

void AnnouncementWhitelist::reloadWhitelist()
{
	QSharedPointer<QFile> f = _file.open();
	if(!f->isOpen())
		return;

	QList<QRegularExpression> whitelist;

	int linen = 0;
	while(true) {
		++linen;
		QByteArray line = f->readLine();
		if(line.isEmpty())
			break;
		line = line.trimmed();
		if(line.isEmpty() || line.startsWith('#'))
			continue;

		QRegularExpression re(QString::fromUtf8(line));
		if(!re.isValid()) {
			logger::warning() << _file.path() << "invalid regular expression on line" << linen;
			continue;
		}

		whitelist << re;
	}

	_whitelist = whitelist;
}
