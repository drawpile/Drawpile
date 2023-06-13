// SPDX-License-Identifier: GPL-3.0-or-later

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

	AvatarListModel(bool autoCommit, QObject *parent=nullptr);

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

public slots:
	void setDefaultAvatarUsername(const QString &username);

private:
	struct Avatar {
		QPixmap icon;
		QString filename;
	};

	bool m_autoCommit;
	QVector<Avatar> m_avatars;
	QStringList m_deletions;
};

#endif
