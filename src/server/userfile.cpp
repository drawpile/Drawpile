/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include "userfile.h"
#include "../shared/util/logger.h"
#include "../shared/util/passwordhash.h"

#include <QFileSystemWatcher>
#include <QFileInfo>

namespace server {

UserFile::UserFile(QObject *parent) :
	IdentityManager(parent), _watcher(new QFileSystemWatcher(this)), _needupdate(true)
{
	connect(_watcher, &QFileSystemWatcher::fileChanged, [this]() { _needupdate = true; });
}

bool UserFile::setFile(const QString &path)
{
	QFileInfo f(path);
	if(!f.isFile() || !f.isReadable()) {
		logger::error() << path << "is not a readable file!";
		return false;
	}
	if((f.permissions() & (QFile::ReadOther|QFile::WriteOther))) {
		logger::warning() << path << "has lax permissions!";
	}

	logger::info() << "Using password file" << path;

	_watcher->addPath(path);
	_path = path;
	_needupdate = true;
	return true;
}

void UserFile::doCheckLogin(const QString &username, const QString &password, IdentityResult *result)
{
	if(_needupdate)
		updateUserFile();

	QString idname = username.toLower();

	if(!_users.contains(idname)) {
		if(password.isEmpty())
			result->setResults(IdentityResult::NOTFOUND, QString(), QStringList());
		else
			result->setResults(IdentityResult::BADPASS, QString(), QStringList());

	} else {
		User u = _users[idname];
		IdentityResult::Status s;
		if(u.passwordhash.startsWith("*")) {
			s = IdentityResult::BANNED;

		} else if(!passwordhash::check(password, u.passwordhash)) {
			s = IdentityResult::BADPASS;

		} else {
			s = IdentityResult::OK;
		}
		result->setResults(s, u.name, u.flags);
	}
}

void UserFile::updateUserFile()
{
	logger::debug() << "Refreshing user list from" << _path;

	QFile f(_path);
	if(!f.open(QFile::ReadOnly)) {
		logger::error() << _path << "Couldn't open password file!" << f.errorString();
		return;
	}

	QHash<QString, User> users;

	int linen = 0;
	while(true) {
		++linen;
		QByteArray line = f.readLine();
		if(line.isEmpty())
			break;
		line = line.trimmed();
		if(line.isEmpty() || line.at(0) == '#')
			continue;

		QList<QByteArray> tokens = line.split(':');
		if(tokens.size()!=3) {
			logger::warning() << _path << "invalid line" << linen << ": expected 3 tokens, got" << tokens.size();
			continue;
		}

		QString username = QString::fromUtf8(tokens.at(0));
		QByteArray passwd = tokens.at(1);
		QStringList flags  = QString::fromUtf8(tokens.at(2)).split(",");

		users[username.toLower()] = {username, flags, passwd};
	}

	_users = users;
	_needupdate = false;
}

}

