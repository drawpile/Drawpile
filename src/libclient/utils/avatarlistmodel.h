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
#ifndef AVATARLISTMODEL_H
#define AVATARLISTMODEL_H

#include <QAbstractListModel>
#include <QVector>
#include <QPixmap>

class AvatarListModel final : public QAbstractListModel
{
	Q_OBJECT
public:
	enum AvatarListRoles {
		FilenameRole = Qt::UserRole + 1,
	};

	AvatarListModel(QObject *parent=nullptr);

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

	Qt::ItemFlags flags(const QModelIndex &index) const override;

	bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

	//! Find avatar by name
	QModelIndex getAvatar(const QString &filename) const;

	//! Reload avatar list and clear all uncommitted changes
	void loadAvatars(bool includeBlank=false);

	//! Add a new avatar
	void addAvatar(const QPixmap &icon);

	//! Save changes
	bool commit();

private:
	struct Avatar {
		QPixmap icon;
		QString filename;
	};

	QVector<Avatar> m_avatars;
	QStringList m_deletions;
};

#endif
