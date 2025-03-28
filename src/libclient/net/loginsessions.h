// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGINSESSIONS_H
#define LOGINSESSIONS_H

#include <QAbstractTableModel>
#include <QVector>

namespace net {

struct LoginSessionVersion {
	QString description;
	bool compatible;
	bool future;
	bool past;
};

/**
 * @brief Available session description
 */
struct LoginSession {
	QString id;
	QString alias;
	QString title;
	QString founder;
	LoginSessionVersion version;

	int userCount;
	int activeDrawingUserCount;

	bool needPassword;
	bool persistent;
	bool newLoginsBlocked;
	bool guestLoginBlocked; // Will only be true if we're a guest.
	bool webLoginBlocked; // Will only be true if we're connected via browser.
	bool unlisted;
	bool nsfm;

	QString idOrAlias() const { return alias.isEmpty() ? id : alias; }
	inline bool isIdOrAlias(const QString &idOrAlias) const
	{
		Q_ASSERT(!idOrAlias.isEmpty());
		return id == idOrAlias || alias == idOrAlias;
	}

	bool isIncompatible() const { return !version.compatible; }
	bool isClosed() const
	{
		return newLoginsBlocked || guestLoginBlocked || webLoginBlocked;
	}

	bool isJoinable(bool mod) const
	{
		return (mod || !isClosed()) && version.compatible;
	}
};

/**
 * @brief List of available sessions
 */
class LoginSessionModel final : public QAbstractTableModel {
	Q_OBJECT
public:
	enum LoginSessionRoles {
		IdRole = Qt::UserRole, // Session ID
		IdAliasRole,		   // ID alias
		AliasOrIdRole,		   // Alias or session ID
		UserCountRole,		   // Number of logged in users
		TitleRole,			   // Session title
		FounderRole,		   // Name of session founder
		NeedPasswordRole,	   // Is a password needed to join
		PersistentRole,		   // Is this a persistent session
		ClosedRole,			   // Is this session closed to new users
		IncompatibleRole, // Is the session meant for some other client version
		JoinableRole,	  // Is this session joinable
		NsfmRole,		  // Is this session tagged as Not Suitable For Minors
		InactiveRole,	  // Does this session have zero active users
		JoinDenyReasonsRole, // Human-readable explanations why they can't join.
		JoinDenyIcon,		 // Icon for the can't join message box.
	};

	enum Column : int {
		ColumnVersion,
		ColumnStatus,
		ColumnTitle,
		ColumnFounder,
		ColumnUsers,
		ColumnActive,
		ColumnCount,
	};

	explicit LoginSessionModel(QObject *parent = nullptr);

	void setModeratorMode(bool mod);
	bool isModeratorMode() const { return m_moderatorMode; }

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	QVariant headerData(
		int section, Qt::Orientation orientation,
		int role = Qt::DisplayRole) const override;

	void updateSession(const LoginSession &session);
	void removeSession(const QString &id);

	LoginSession getFirstSession() const
	{
		return m_sessions.isEmpty() ? LoginSession() : m_sessions.first();
	}

	static QStringList getJoinDenyReasons(
		bool newLoginsBlocked, bool guestLoginBlocked, bool webLoginBlocked,
		bool incompatibleVersion, bool futureVersion, bool pastVersion);

	static QIcon getJoinDenyIcon(bool incompatibleVersion);

private:
	bool isNsfm(const LoginSession &ls) const;

	QVector<LoginSession> m_sessions;
	bool m_moderatorMode;
};

}

#endif
