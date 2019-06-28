/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

#ifndef PASSWORDSTORE_H
#define PASSWORDSTORE_H

#include <QString>
#include <QHash>

class QJsonDocument;
class QStandardItemModel;
class QObject;

/**
 * @brief User password storage for server and ext-auth logins.
 *
 * TODO: passwords are stored unencrypted. Best solution would be to integrate
 * with platform specific keyrings. Perhaps turn this into a base class and add
 * alternative backends, falling back to unencrypted file storage only if
 * a better one is not available.
 */
class PasswordStore {
public:
	typedef QHash<QString, QString> PasswordEntries; // username -> password mapping
	typedef QHash<QString, PasswordEntries> PasswordMap; // server name -> username/password list mapping

	enum class Type { Server, Extauth };

	//! Construct a password store with the default password file location
	PasswordStore();

	//! Construct a password store with an explicitly set password file
	explicit PasswordStore(const QString &passwordFilePath);

	//! Load the password file into memory
	bool load(QString *errorMessage=nullptr);

	//! Save the password file
	bool save(QString *errorMessage=nullptr) const;

	//! Save the password file to some other path
	bool saveAs(const QString &path, QString *errorMessage=nullptr) const;

	/**
	 * @brief Get a password for the given server/user/type triple
	 *
	 * If type is Extauth, the server should be the domain name of the extauth server.
	 *
	 * @param server domain or IP address of the server we're authenticating against
	 * @param username username
	 * @param type authentication type
	 * @return password or empty string if no password was saved
	 */
	QString getPassword(const QString &server, const QString &username, Type type) const;

	/**
	 * @brief Set a password for the given server/user/type triple
	 *
	 * Remember to call save() afterwards to persist the changes.
	 *
	 * @param server server domain or IP
	 * @param username username
	 * @param password the password to store
	 * @param type authentication type
	 */
	void setPassword(const QString &server, const QString &username, Type type, const QString &password);

	/**
	 * @brief Forget the password for the given server/user/type triple
	 *
	 * Remember to call save() afterwards to persist the changes.
	 *
	 * @param server server domain or IP
	 * @param username username
	 * @param type authentication type
	 * @return true if a password was found in the list and deleted
	 */
	bool forgetPassword(const QString &server, const QString &username, Type type);

	/**
	 * @brief Get the content of the password store in JSON format
	 */
	QJsonDocument toJsonDocument() const;

	/**
	 * @brief Get the list of stored passwords as a tree model.
	 *
	 * The actual passwords are not included.
	 */
	QStandardItemModel *toStandardItemModel(QObject *parent=nullptr) const;

private:
	PasswordMap m_serverPasswords;
	PasswordMap m_extauthPasswords;
	QString m_passwordFilePath;
};


#endif

