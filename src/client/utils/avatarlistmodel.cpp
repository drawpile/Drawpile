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
#include "avatarlistmodel.h"

#include <QStandardPaths>
#include <QDir>

AvatarListModel::AvatarListModel(QObject *parent)
	: QAbstractListModel(parent)
{
}

int AvatarListModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return m_avatars.size();
}

QVariant AvatarListModel::data(const QModelIndex &index, int role) const
{
	if(!index.isValid() || index.row() < 0 || index.row() >= m_avatars.size())
		return QVariant();

	const Avatar &a = m_avatars.at(index.row());

	switch(role) {
	case Qt::DisplayRole: return a.name;
	case Qt::DecorationRole: return a.icon;
	}

	return QVariant();
}

Qt::ItemFlags AvatarListModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags f = Qt::NoItemFlags;
	if(index.isValid() && index.row() >= 0 && index.row() < m_avatars.size()) {
		f = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
	}
	return f;
}

bool AvatarListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if(role!=Qt::EditRole || !index.isValid() || index.row() < 0 || index.row() >= m_avatars.size())
		return false;

	const QString newName = value.toString();

	// Name must be unique and non-empty
	if(newName.isEmpty())
		return false;

	for(const Avatar &a : m_avatars) {
		if(a.name == newName)
			return false;
	}

	m_avatars[index.row()].name = newName;

	emit dataChanged(index, index);
	return true;
}

bool AvatarListModel::removeRows(int row, int count, const QModelIndex &parent)
{
	if(row<0 || row + count > m_avatars.size() || parent.isValid())
		return false;

	beginRemoveRows(QModelIndex(), row, row+count-1);
	while(count--) {
		if(!m_avatars[row].originalName.isEmpty())
			m_deletions << m_avatars[row].originalName;
		m_avatars.remove(row);
	}
	endRemoveRows();
	return true;
}

void AvatarListModel::addAvatar(const QString &name, const QPixmap &icon)
{
	// TODO sort alphabetically?
	beginInsertRows(QModelIndex(), m_avatars.size(), m_avatars.size());
	m_avatars << Avatar {
		icon,
		name,
		QString() // No original name since file hasn't been saved yet
	};
	endInsertRows();
}

void AvatarListModel::loadAvatars()
{
	QVector<Avatar> avatars;

	QDir dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
	if(!dir.cd("avatars"))
		return;

	const QStringList files = dir.entryList(QStringList() << "*.png", QDir::Files|QDir::Readable);

	for(const QString &filename : files) {
		const QString name = filename.left(filename.lastIndexOf('.'));
		avatars << Avatar {
			QPixmap(dir.filePath(filename), "PNG"),
			name,
			name
		};
	}

	beginResetModel();
	m_avatars = avatars;
	m_deletions.clear();
	endResetModel();
}

bool AvatarListModel::commit()
{
	QDir dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
	dir.mkdir("avatars");
	if(!dir.cd("avatars")) {
		qWarning("Couldn't create avatar directory: %s/avatars", qPrintable(dir.path()));
		return false;
	}

	// Commit deletions
	for(const QString &name : m_deletions) {
		dir.remove(name + ".png");
	}

	// Rename avatars and save new ones
	QStringList renames;
	for(const Avatar &a : m_avatars) {
		if(a.name != a.originalName) {
			if(a.originalName.isEmpty()) {
				// Newly added avatar
				if(!a.icon.save(dir.filePath(a.name + ".png"), "PNG")) {
					qWarning("Couldn't save %s.png", qPrintable(a.name));
				}

			} else {
				// Rename avatar
				const QString newName = a.name + ".png";
				if(dir.exists(newName)) {
					// Uh oh, we have swapped filenames.
					// Rename this in two passes.
					if(!dir.rename(a.originalName + ".png", QString("_rename%1").arg(renames.size()))) {
						qWarning("Renaming %s failed (first pass)!", qPrintable(a.originalName));
						return false;
					}
					renames << newName;

				} else {
					if(!dir.rename(a.originalName + ".png", newName)) {
						qWarning("Renaming %s failed!", qPrintable(a.originalName));
						return false;
					}
				}
			}
		}
	}

	// Second pass renames
	for(int i=0;i<renames.size();++i) {
		if(!dir.rename(QString("_rename%1").arg(i), renames.at(i))) {
			qWarning("Renaming _rename%d failed!", i);
			return false;
		}
	}

	return true;
}
