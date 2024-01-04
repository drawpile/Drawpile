// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGINSESSIONS_H
#define LOGINSESSIONS_H

#include <QAbstractTableModel>
#include <QVector>

namespace net {

/**
 * @brief Available session description
 */
struct LoginSession {
	QString id;
	QString alias;
	QString title;
	QString founder;
	QString incompatibleSeries; // if not empty, this session is for a different version series
	bool compatibilityMode;

	int userCount;
	int activeDrawingUserCount;

	bool needPassword;
	bool persistent;
	bool newLoginsBlocked;
	bool guestLoginBlocked; // Will only be true if we're a guest.

	bool nsfm;

	QString idOrAlias() const { return alias.isEmpty() ? id : alias; }
	inline bool isIdOrAlias(const QString &idOrAlias) const {
		Q_ASSERT(!idOrAlias.isEmpty());
		return id == idOrAlias || alias == idOrAlias;
	}

	bool isIncompatible() const { return !incompatibleSeries.isEmpty(); }
	bool isClosed() const { return newLoginsBlocked || guestLoginBlocked; }
};

/**
 * @brief List of available sessions
 */
class LoginSessionModel final : public QAbstractTableModel
{
	Q_OBJECT
public:
	enum LoginSessionRoles {
		IdRole = Qt::UserRole,     // Session ID
		IdAliasRole,               // ID alias
		AliasOrIdRole,             // Alias or session ID
		UserCountRole,             // Number of logged in users
		TitleRole,                 // Session title
		FounderRole,               // Name of session founder
		NeedPasswordRole,          // Is a password needed to join
		PersistentRole,            // Is this a persistent session
		ClosedRole,                // Is this session closed to new users
		IncompatibleRole,          // Is the session meant for some other client version
		JoinableRole,              // Is this session joinable
		NsfmRole,                  // Is this session tagged as Not Suitable For Minors
		CompatibilityModeRole,     // Is this a Drawpile 2.1.x session
		InactiveRole,              // Does this session have zero active users
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

	explicit LoginSessionModel(QObject *parent=nullptr);

	void setModeratorMode(bool mod);
	bool isModeratorMode() const { return m_moderatorMode; }

	int rowCount(const QModelIndex &parent=QModelIndex()) const override;
	int columnCount(const QModelIndex &parent=QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role=Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;

	void updateSession(const LoginSession &session);
	void removeSession(const QString &id);

	LoginSession getFirstSession() const { return m_sessions.isEmpty() ? LoginSession() : m_sessions.first(); }

private:
	bool isNsfm(const LoginSession &ls) const;

	QVector<LoginSession> m_sessions;
	bool m_moderatorMode;
};

}

#endif
