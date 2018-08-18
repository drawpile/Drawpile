/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2018 Calle Laakkonen

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
	QString alias;
	QString title;
	QString founder;

	int userCount;

	bool needPassword;
	bool persistent;
	bool closed;
	bool incompatible;
	bool nsfm;

	QString idOrAlias() const { return alias.isEmpty() ? id : alias; }
	inline bool isIdOrAlias(const QString &idOrAlias) const {
		Q_ASSERT(!idOrAlias.isEmpty());
		return id == idOrAlias || alias == idOrAlias;
	}
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
		IdAliasRole,               // ID alias
		UserCountRole,             // Number of logged in users
		TitleRole,                 // Session title
		FounderRole,               // Name of session founder
		NeedPasswordRole,          // Is a password needed to join
		PersistentRole,            // Is this a persistent session
		ClosedRole,                // Is this session closed to new users
		IncompatibleRole,          // Is the session meant for some other client version
		JoinableRole,              // Is this session joinable
		NsfmRole                   // Is this session tagged as Not Suitable For Minors
	};

	explicit LoginSessionModel(QObject *parent=nullptr);

	int rowCount(const QModelIndex &parent=QModelIndex()) const;
	int columnCount(const QModelIndex &parent=QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role=Qt::DisplayRole) const;
	Qt::ItemFlags flags(const QModelIndex &index) const;

	LoginSession sessionAt(int index) const { return m_filtered.at(index); }
	void updateSession(const LoginSession &session);
	void removeSession(const QString &id);

	void setShowNsfm(bool show);
	int filteredCount() const { return m_sessions.size() - m_filtered.size(); }

	//! Get the ID of the first session
	LoginSession getFirstSession() const { return m_sessions.isEmpty() ? LoginSession() : m_sessions.first(); }

signals:
	void filteredCountChanged();

private:
	void updateFiltered(const LoginSession &session);
	void removeFiltered(const QString &id);

	QList<LoginSession> m_sessions;
	QList<LoginSession> m_filtered;
	bool m_showNsfm;
};

}

#endif
