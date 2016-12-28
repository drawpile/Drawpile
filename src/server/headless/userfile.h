/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2015 Calle Laakkonen

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

#ifndef USERFILE_H
#define USERFILE_H

#include "updateablefile.h"
#include "../shared/server/identitymanager.h"

#include <QHash>

namespace server {
namespace headless {

class UserFile : public IdentityManager
{
	Q_OBJECT
public:
	explicit UserFile(QObject *parent = 0);

	bool setFile(const QString &path);

protected:
	void doCheckLogin(const QString &username, const QString &password, IdentityResult *result);

private:
	void updateUserFile();

	struct User {
		QString name;
		QStringList flags;
		QByteArray passwordhash;
	};

	UpdateableFile _file;
	QHash<QString, User> _users;

};

}
}

#endif // USERFILE_H
