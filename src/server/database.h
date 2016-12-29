/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2016 Calle Laakkonen

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

#ifndef SERVER_CFG_DATABASE_H
#define SERVER_CFG_DATABASE_H

#include "../shared/server/serverconfig.h"

namespace server {

/**
 * @brief Configuration database access object.
 *
 *
 */
class Database : public ServerConfig
{
	Q_OBJECT
public:
	explicit Database(QObject *parent=nullptr);
	~Database();

	bool openFile(const QString &path);

	void setConfigString(ConfigKey key, const QString &value) override;

	bool isAllowedAnnouncementUrl(const QUrl &url) const override;
	bool isAddressBanned(const QHostAddress &addr) const override;
	RegisteredUser getUserAccount(const QString &username, const QString &password) const override;

protected:
	QString getConfigValue(const ConfigKey key, bool &found) const override;

private:
	struct Private;
	Private *d;
};

}

#endif // DATABASE_H
