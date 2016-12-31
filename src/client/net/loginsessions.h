/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2016 Calle Laakkonen

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

#ifndef LOGINSESSIONS_H
#define LOGINSESSIONS_H

#include <QAbstractTableModel>

namespace net {

/**
 * @brief Available session description
 */
struct LoginSession {
	QString id;
	bool customId;
	int userCount;
	QString title;
	QString founder;

	bool needPassword;
	bool persistent;
	bool closed;
	bool incompatible;

	LoginSession() : customId(false), userCount(0), needPassword(false), persistent(false), closed(false), incompatible(false) { }
};

/**
 * @brief List of available sessions
 */
class LoginSessionModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	enum LoginSessionRoles {
		IdRole = Qt::UserRole + 1, // Session ID
		IsCustomIdRole,            // Is this a manually selected ID?
		UserCountRole,             // Number of logged in users
		TitleRole,                 // Session title
		FounderRole,               // Name of session founder
		NeedPasswordRole,          // Is a password needed to join
		PersistentRole,            // Is this a persistent session
		ClosedRole,                // Is this session closed to new users
		IncompatibleRole,          // Is the session meant for some other client version
		JoinableRole               // Is this session joinable
	};

	explicit LoginSessionModel(QObject *parent = 0);

	int rowCount(const QModelIndex &parent=QModelIndex()) const;
	int columnCount(const QModelIndex &parent=QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role=Qt::DisplayRole) const;
	Qt::ItemFlags flags(const QModelIndex &index) const;

	LoginSession sessionAt(int index) const { return m_sessions.at(index); }
	void updateSession(const LoginSession &session);
	void removeSession(const QString &id);

private:
	QList<LoginSession> m_sessions;
};

}

#endif
