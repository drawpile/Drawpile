// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/utils/avatarlistmodel.h"
#include "libclient/utils/identicon.h"
#include "libshared/util/paths.h"

#include <QDir>
#include <QBuffer>
#include <QCryptographicHash>

AvatarListModel::AvatarListModel(bool autoCommit, QObject *parent)
	: QAbstractListModel(parent)
	, m_autoCommit(autoCommit)
{
}

int AvatarListModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return m_avatars.size();
}

void AvatarListModel::setDefaultAvatarUsername(const QString &username)
{
	auto &avatar = m_avatars.first();
	avatar.icon = QPixmap::fromImage(make_identicon(username));
	const auto index = createIndex(0, 0);
	emit dataChanged(index, index, { Qt::DecorationRole });
}

QVariant AvatarListModel::data(const QModelIndex &index, int role) const
{
	if(!index.isValid() || index.row() < 0 || index.row() >= m_avatars.size())
		return QVariant();

	const Avatar &a = m_avatars.at(index.row());

	switch(role) {
	case Qt::DisplayRole: return a.icon.isNull() ? a.filename : QVariant();
	case Qt::DecorationRole: return a.icon;
	case FilenameRole: return a.filename;
	case TypeRole: return a.type;
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

	if(m_autoCommit) {
		commit();
	}
	return true;
}

QModelIndex AvatarListModel::getAvatar(const QString &filename) const
{
	for(int i=0;i<m_avatars.size();++i) {
		const Avatar &avatar = m_avatars[i];
		if(avatar.type == FileAvatar && avatar.filename == filename) {
			return index(i);
		}
	}
	return QModelIndex();
}

void AvatarListModel::addAvatar(const QPixmap &icon)
{
	// Add the new avatar before the potentially trailing "add avatar" option.
	int count = m_avatars.size();
	int i = count > 0 && m_avatars[count - 1].type == Type::AddAvatar
				? count - 1
				: count;

	beginInsertRows(QModelIndex(), i, i);
	m_avatars.insert(i, Avatar{Type::FileAvatar, icon, QString()});
	endInsertRows();

	if(m_autoCommit) {
		commit();
	}
}

void AvatarListModel::loadAvatars(bool includeBlank, bool includeAdd)
{
	QVector<Avatar> avatars;

	if(includeBlank) {
		avatars << Avatar {
			Type::NoAvatar,
			QPixmap(),
			tr("No avatar")
		};
	}

	QDir dir = utils::paths::writablePath("avatars");
	const QStringList files = dir.entryList(QStringList() << "*.png", QDir::Files|QDir::Readable);

	for(const QString &filename : files) {
		avatars << Avatar {
			Type::FileAvatar,
			QPixmap(dir.filePath(filename), "PNG"),
			filename
		};
	}

	if(includeAdd) {
		avatars << Avatar {
			Type::AddAvatar,
			QPixmap(),
			tr("Add avatar…")
		};
	}

	beginResetModel();
	m_avatars = avatars;
	m_deletions.clear();
	endResetModel();
}

void AvatarListModel::commit()
{
	QDir dir = utils::paths::writablePath("avatars", ".");

	// Commit deletions
	for(const QString &filename : m_deletions) {
		dir.remove(filename);
	}
	m_deletions.clear();

	// Save newly added avatars
	int count = m_avatars.size();
	for(int i = 0; i < count; ++i) {
		Avatar &a = m_avatars[i];
		if(a.type == Type::FileAvatar && !a.icon.isNull() && a.filename.isEmpty()) {
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
				qWarning("Couldn't open %s", qUtf8Printable(filename));
				continue;
			}

			if(f.write(buf.data()) == buf.size() && f.flush()) {
				f.close();
				a.filename = filename;
				QModelIndex index = createIndex(i, 0);
				dataChanged(index, index, {Qt::DisplayRole, FilenameRole});
			} else {
				qWarning("Couldn't write %s", qUtf8Printable(filename));
				f.close();
				f.remove();
			}
		}
	}
}
