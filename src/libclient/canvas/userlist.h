// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DP_NET_USERLISTMODEL_H
#define DP_NET_USERLISTMODEL_H
#include "libclient/net/message.h"
#include <QAbstractTableModel>
#include <QList>
#include <QPixmap>
#include <QSortFilterProxyModel>
#include <optional>

class QJsonArray;

namespace canvas {

class AclState;

/**
 * @brief Information about a user
 */
struct User {
	int id;
	QString name;
	QPixmap avatar;
	bool isLocal;
	bool isOperator;
	bool isTrusted;
	bool isMod;
	bool isBot;
	bool isAuth;
	bool isLocked;
	bool isMuted;
	bool isOnline;
	bool isMinorIncompatibility;
};

class OnlineUserListModel;

/**
 * A list model to represent session users.
 */
class UserListModel final : public QAbstractTableModel {
	Q_OBJECT
public:
	enum UserListRoles {
		IdRole = Qt::UserRole + 1,
		NameRole,
		AvatarRole,
		IsOpRole,
		IsTrustedRole,
		IsModRole,
		IsAuthRole,
		IsBotRole,
		IsLockedRole,
		IsMutedRole,
		IsOnlineRole,
		IsMinorIncompatibilityRole,
	};

	UserListModel(QObject *parent = nullptr);

	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant headerData(
		int section, Qt::Orientation orientation,
		int role = Qt::DisplayRole) const override;

	//! A new user logs in
	void userLogin(const User &user);

	//! A user logs out (marked as offline)
	void userLogout(int id);

	//! Mark all users as offline
	void allLogout();

	//! Clear all users and history
	void reset();

	//! Get all users (includes logged out users)
	const QVector<User> &users() const { return m_users; }
	void setUsers(const QVector<User> &users);

	//! Get a shared instance of a filtered model that only lists online users
	OnlineUserListModel *onlineUsers() const { return m_onlineUsers; }

	/**
	 * @brief Get user info by ID
	 *
	 * This will return info about past users as well.
	 * @param id user id
	 * @return
	 */
	User getUserById(int id) const;

	std::optional<User> getOptionalUserById(int id) const;

	/**
	 * @brief Get the name of the user with the given context ID
	 *
	 * If no such user exists, "User #X" is returned, where X is the ID number.
	 * @param id
	 * @return user name
	 */
	QString getUsername(int id) const;

	//! Get a list of users with operator privileges
	QVector<uint8_t> operatorList() const;

	//! Get a list of users who are locked
	QVector<uint8_t> lockList() const;

	//! Get a list of trusted users
	QVector<uint8_t> trustedList() const;

	/**
	 * @brief Get the command for (un)locking a single user
	 * @param localId
	 * @param userId
	 * @param lock
	 * @return
	 */
	net::Message getLockUserCommand(int localId, int userId, bool lock) const;

	/**
	 * @brief Get the command for granting or revoking operator privileges
	 * @param localId
	 * @param userId
	 * @param op
	 * @return
	 */
	net::Message getOpUserCommand(int localId, int userId, bool op) const;

	/**
	 * @brief Get the command for granting or revoking trusted status
	 * @param localId
	 * @param userId
	 * @param trusted
	 * @return
	 */
	net::Message getTrustUserCommand(int localId, int userId, bool op) const;

	/**
	 * @brief Check if the given user is an operator
	 * @param userId
	 * @return
	 */
	bool isOperator(int userId) const;

public slots:
	void updateAclState(const AclState *state);
	void updateMuteList(const QJsonArray &mutedUserIds);

private:
	QVector<User> m_users;
	OnlineUserListModel *m_onlineUsers;
};

/**
 * A filtered user list model that only includes online users
 */
class OnlineUserListModel final : public QSortFilterProxyModel {
	Q_OBJECT
public:
	using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
	bool filterAcceptsRow(
		int source_row, const QModelIndex &source_parent) const override;
};

}

Q_DECLARE_METATYPE(canvas::User)

#endif
