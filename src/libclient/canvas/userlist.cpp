// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpmsg/message.h>
}

#include "libclient/canvas/userlist.h"
#include "libclient/utils/icon.h"
#include "libclient/canvas/acl.h"

#include <QDebug>
#include <QJsonArray>
#include <QPixmap>
#include <QPalette>

namespace canvas {

UserListModel::UserListModel(QObject *parent)
	: QAbstractTableModel(parent)
{
	m_onlineUsers = new OnlineUserListModel(this);
	m_onlineUsers->setSourceModel(this);
}


QVariant UserListModel::data(const QModelIndex& index, int role) const
{
	if(!index.isValid() || index.row() < 0 || index.row() >= m_users.size())
		return QVariant();

	const User &u = m_users.at(index.row());

	if(role == Qt::ForegroundRole && !u.isOnline) {
		return QPalette().color(QPalette::ColorGroup::Disabled, QPalette::ColorRole::WindowText);
	}

	if(index.column() == 0) {
		switch(role) {
			case IdRole: return u.id;
			case Qt::DisplayRole:
			case NameRole: return u.name;
			case Qt::DecorationRole:
			case AvatarRole: return u.avatar;
			case IsOpRole: return u.isOperator;
			case IsTrustedRole: return u.isTrusted;
			case IsModRole: return u.isMod;
			case IsAuthRole: return u.isAuth;
			case IsBotRole: return u.isBot;
			case IsLockedRole: return u.isLocked;
			case IsMutedRole: return u.isMuted;
			case IsOnlineRole: return u.isOnline;
		}

	} else if(role==Qt::DisplayRole) {
		switch(index.column()) {
		case 1:
			if(u.isMod)
				return tr("Moderator");
			else if(u.isOperator)
				return tr("Operator");
			else if(u.isTrusted)
				return tr("Trusted");
			else if(u.isAuth)
				return tr("Registered");
			else
				return QVariant();

		case 2: return u.isOnline ? tr("Online") : tr("Offline");
		}

	} else if(role==Qt::DecorationRole) {
		switch(index.column()) {
		case 3: return u.isLocked ? icon::fromTheme("object-locked") : QVariant();
		case 4: return u.isMuted ? icon::fromTheme("irc-unvoice") : QVariant();
		}
	}

	return QVariant();
}

int UserListModel::columnCount(const QModelIndex&) const
{
	return 5;
}

int UserListModel::rowCount(const QModelIndex& parent) const
{
	if(parent.isValid())
		return 0;
	return m_users.count();
}

QVariant UserListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(orientation == Qt::Horizontal) {
		if(role == Qt::DisplayRole) {
			switch(section) {
			case 0: return tr("User");
			case 1: return tr("Type");
			case 2: return tr("Status");
			}
		} else if(role == Qt::DecorationRole) {
			switch(section) {
			case 3: return icon::fromTheme("object-locked");
			case 4: return icon::fromTheme("irc-unvoice");
			}
		}

	} else if(section >=0 && section < m_users.size() && role == Qt::DisplayRole) {
		return m_users.at(section).id;
	}

	return QVariant();
}

void UserListModel::userLogin(const User &user)
{
	// Check if this is a returning user
	for(int i=0;i<m_users.count();++i) {
		User &u = m_users[i];
		if(u.id == user.id) {
			u.name = user.name;
			u.avatar = user.avatar;
			u.isLocal = user.isLocal;
			u.isAuth = user.isAuth;
			u.isMod = user.isMod;
			u.isBot = user.isBot;
			u.isMuted = user.isMuted;
			u.isOnline = true;

			emit dataChanged(index(i, 0), index(i, columnCount()-1));
			return;
		}
	}

	// Add new user
	int pos = m_users.count();
	beginInsertRows(QModelIndex(),pos,pos);
	m_users.append(user);
	endInsertRows();
}

void UserListModel::userLogout(int id)
{
	for(int i=0;i<m_users.size();++i) {
		if(m_users.at(i).id == id) {
			m_users[i].isOnline = false;
			emit dataChanged(index(i, 0), index(i, columnCount()-1));
			return;
		}
	}
}

void UserListModel::allLogout()
{
	if(!m_users.isEmpty()) {
		for(int i=0;i<m_users.size();++i)
			m_users[i].isOnline = false;
		emit dataChanged(
			index(0, 0),
			index(m_users.size()-1, columnCount()-1)
		);
	}
}

void UserListModel::reset()
{
	int size = m_users.size();
	if(size != 0) {
		beginRemoveRows(QModelIndex(), 0, size - 1);
		m_users.clear();
		endRemoveRows();
	}
}

void UserListModel::updateAclState(const AclState *acl)
{
	for(int i=0;i<m_users.size();++i) {
		User &u = m_users[i];

		bool changed = false;

		const bool op = acl->isOperator(u.id);
		const bool trusted = acl->isTrusted(u.id);
		const bool locked = acl->isLocked(u.id);

		if(op != u.isOperator) {
			u.isOperator = op;
			changed = true;
		}

		if(trusted != u.isTrusted) {
			u.isTrusted = trusted;
			changed = true;
		}

		if(locked != u.isLocked) {
			u.isLocked = locked;
			changed = true;
		}

		if(changed)
			emit dataChanged(index(i, 0), index(i, columnCount()-1));
	}
}

void UserListModel::updateMuteList(const QJsonArray &mutedUserIds)
{
	for(int i=0;i<m_users.size();++i) {
		User &u = m_users[i];
		const bool mute = mutedUserIds.contains(u.id);
		if(u.isMuted != mute) {
			u.isMuted = mute;
			emit dataChanged(index(i, 0), index(i, columnCount()-1));
		}
	}
}

QVector<uint8_t> UserListModel::operatorList() const
{
	QVector<uint8_t> ops;
	for(int i=0;i<m_users.size();++i) {
		const User &u = m_users.at(i);
		if(u.isOnline && (u.isOperator || u.isMod))
			ops << u.id;
	}
	return ops;
}

QVector<uint8_t> UserListModel::lockList() const
{
	QVector<uint8_t> locks;
	for(int i=0;i<m_users.size();++i) {
		const User &u = m_users.at(i);
		if(u.isOnline && u.isLocked)
			locks << m_users.at(i).id;
	}
	return locks;
}

QVector<uint8_t> UserListModel::trustedList() const
{
	QVector<uint8_t> ids;
	for(int i=0;i<m_users.size();++i) {
		const User &u = m_users.at(i);
		if(u.isOnline && u.isTrusted)
			ids << m_users.at(i).id;
	}
	return ids;
}

User UserListModel::getUserById(int id) const
{
	return getOptionalUserById(id).value_or(User{});
}

std::optional<User> UserListModel::getOptionalUserById(int id) const
{
	for(const User &u : m_users) {
		if(u.id == id) {
			return u;
		}
	}
	return {};
}

bool UserListModel::isOperator(int userId) const
{
	for(const User &u : m_users) {
		if(u.id == userId) {
			return u.isOperator;
		}
	}
	return false;
}

QString UserListModel::getUsername(int id) const
{
	// Special case: id 0 is reserved for the server
	if(id==0)
		return tr("Server");

	for(const User &u : m_users)
		if(u.id == id)
			return u.name;

	// Not found
	return tr("User #%1").arg(id);
}

drawdance::Message UserListModel::getLockUserCommand(int localId, int userId, bool lock) const
{
	Q_ASSERT(userId>0 && userId<255);

	QVector<uint8_t> ids = lockList();
	if(lock) {
		if(!ids.contains(userId))
			ids.append(userId);
	} else {
		ids.removeAll(userId);
	}

	return drawdance::Message::makeUserAcl(localId, ids);
}

drawdance::Message UserListModel::getOpUserCommand(int localId, int userId, bool op) const
{
	Q_ASSERT(userId>0 && userId<255);

	QVector<uint8_t> ops = operatorList();
	if(op) {
		if(!ops.contains(userId))
			ops.append(userId);
	} else {
		ops.removeOne(userId);
	}

	return drawdance::Message::makeSessionOwner(localId, ops);
}

drawdance::Message UserListModel::getTrustUserCommand(int localId, int userId, bool trust) const
{
	Q_ASSERT(userId>0 && userId<255);

	QVector<uint8_t> trusted = trustedList();
	if(trust) {
		if(!trusted.contains(userId))
			trusted.append(userId);
	} else {
		trusted.removeOne(userId);
	}

	return drawdance::Message::makeTrustedUsers(localId, trusted);
}

bool OnlineUserListModel::filterAcceptsRow(int source_row, const QModelIndex &parent) const
{
	const auto i = sourceModel()->index(source_row, 0);
	return i.data(UserListModel::IsOnlineRole).toBool() && QSortFilterProxyModel::filterAcceptsRow(source_row, parent);
}

}
