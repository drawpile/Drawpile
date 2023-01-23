/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018-2019 Calle Laakkonen

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
#include "libclient/utils/avatarlistmodel.h"
#include "libshared/util/paths.h"

#include <QDir>
#include <QBuffer>
#include <QCryptographicHash>

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
	case Qt::DisplayRole: return a.icon.isNull() ? a.filename : QString();
	case Qt::DecorationRole: return a.icon;
	case FilenameRole: return a.filename;
	}

	return QVariant();
}

Qt::ItemFlags AvatarListModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags f = Qt::NoItemFlags;
	if(index.isValid() && index.row() >= 0 && index.row() < m_avatars.size()) {
		f = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
	}
	return f;
}

bool AvatarListModel::removeRows(int row, int count, const QModelIndex &parent)
{
	if(row<0 || row + count > m_avatars.size() || parent.isValid())
		return false;

	beginRemoveRows(QModelIndex(), row, row+count-1);
	while(count--) {
		if(!m_avatars[row].filename.isEmpty())
			m_deletions << m_avatars[row].filename;
		m_avatars.remove(row);
	}
	endRemoveRows();
	return true;
}

QModelIndex AvatarListModel::getAvatar(const QString &filename) const
{
	for(int i=0;i<m_avatars.size();++i) {
		if(m_avatars.at(i).filename == filename)
			return index(i);
	}

	return QModelIndex();
}

void AvatarListModel::addAvatar(const QPixmap &icon)
{
	beginInsertRows(QModelIndex(), m_avatars.size(), m_avatars.size());
	m_avatars << Avatar {
		icon,
		QString()
	};
	endInsertRows();
}

void AvatarListModel::loadAvatars(bool includeBlank)
{
	QVector<Avatar> avatars;

	if(includeBlank) {
		avatars << Avatar {
			QPixmap(),
			tr("No avatar")
		};
	}

	QDir dir = utils::paths::writablePath("avatars");
	const QStringList files = dir.entryList(QStringList() << "*.png", QDir::Files|QDir::Readable);

	for(const QString &filename : files) {
		avatars << Avatar {
			QPixmap(dir.filePath(filename), "PNG"),
			filename
		};
	}

	beginResetModel();
	m_avatars = avatars;
	m_deletions.clear();
	endResetModel();
}

bool AvatarListModel::commit()
{
	QDir dir = utils::paths::writablePath("avatars", ".");

	// Commit deletions
	for(const QString &filename : m_deletions) {
		dir.remove(filename);
	}
	m_deletions.clear();

	// Save newly added avatars
	for(Avatar &a : m_avatars) {
		if(!a.icon.isNull() && a.filename.isEmpty()) {
			Q_ASSERT(!a.icon.isNull());

			QBuffer buf;
			buf.open(QBuffer::ReadWrite);

			a.icon.save(&buf, "PNG");
			buf.seek(0);

			QCryptographicHash hash(QCryptographicHash::Md5);
			hash.addData(&buf);

			const QString filename = QString::fromUtf8(hash.result().toHex() + ".png");
			QFile f { dir.filePath(filename) };
			if(!f.open(QFile::WriteOnly)) {
				qWarning("Couldn't save %s", qPrintable(filename));
				return false;
			}
			f.write(buf.data());
			f.close();

			a.filename = filename;
		}
	}
	return true;
}
