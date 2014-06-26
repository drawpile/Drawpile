/*
   DrawPile - a collaborative drawing program.

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

#ifndef IDENTITYMANAGER_H
#define IDENTITYMANAGER_H

#include <QObject>
#include <QFuture>

namespace server {

struct IdentityResult {
	enum {
		NOTFOUND, // username not found, guest login possible
		BADPASS,  // wrong password, cannot login
		BANNED,   // user has been banned, cannot login
		OK        // username and password correct, can login as identified user
	} status;

	QString canonicalName;
	QStringList flags;
};

class IdentityManager : public QObject
{
	Q_OBJECT
public:
	explicit IdentityManager(QObject *parent = 0);

	QFuture<IdentityResult> checkLogin(const QString &username, const QString &password);

	/**
	 * @brief Set whether only authorized users are allowed to log in
	 * @param allow
	 */
	void setAuthorizedOnly(bool needauth) { _needauth = needauth; }
	bool isAuthorizedOnly() const { return _needauth; }

protected:
	/**
	 * @brief Check if the given username/password combination is valid
	 *
	 * Note. This function is called by checkLogin() in a separate thread!
	 *
	 * @param username
	 * @param password
	 * @return
	 */
	virtual IdentityResult doCheckLogin(const QString &username, const QString &password) = 0;

private:
	bool _needauth;
};

}

#endif // IDENTITYMANAGER_H
