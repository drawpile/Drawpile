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

#ifndef LOGINSESSIONS_H
#define LOGINSESSIONS_H

#include <QAbstractTableModel>

namespace net {

/**
 * @brief Available session description
 */
struct LoginSession {
	QString id;
	int userCount;
	QString title;
	QString founder;

	bool needPassword;
	bool persistent;
	bool closed;
	bool asleep;
	bool incompatible;

	LoginSession() : userCount(0), needPassword(false), persistent(false), closed(false), asleep(false), incompatible(false) { }
};

/**
 * @brief List of available sessions
 */
class LoginSessionModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	explicit LoginSessionModel(QObject *parent = 0);

	int rowCount(const QModelIndex &parent=QModelIndex()) const;
	int columnCount(const QModelIndex &parent=QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role=Qt::DisplayRole) const;

	LoginSession sessionAt(int index) const { return _sessions.at(index); }
	void updateSession(const LoginSession &session);
	void removeSession(const QString &id);

private:
	QList<LoginSession> _sessions;
};

}

#endif
