// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef AUTHLISTMODEL_H
#define AUTHLISTMODEL_H
#include <QAbstractTableModel>
#include <QStringList>
#include <QVector>

class QJsonArray;

namespace net {

struct AuthListEntry {
	QString authId;
	QString username;
	bool op;
	bool trusted;
	bool mod;
};

class AuthListModel final : public QAbstractTableModel {
	Q_OBJECT
public:
	enum {
		AuthIdRole = Qt::UserRole,
		IsOpRole,
		IsTrustedRole,
		IsModRole,
		IsOwnRole,
	};

	explicit AuthListModel(QObject *parent = nullptr);

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;

	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

	QVariant headerData(
		int section, Qt::Orientation orientation,
		int role = Qt::DisplayRole) const override;

	bool isEmpty() const { return m_list.isEmpty(); }
	const QVector<AuthListEntry> &entries() const { return m_list; }

	// Update received from server when online.
	void update(const QJsonArray &auth);

	// Load from import when offline.
	void load(const QJsonArray &auth);

	void clear();

	void setIsOperator(bool isOperator);

	void setOwnAuthId(const QString &authId);

	void setIndexIsOp(const QModelIndex &index, bool op);
	void setIndexIsTrusted(const QModelIndex &index, bool trusted);

signals:
	void updateApplied();

private:
	void addOrUpdateEntry(const AuthListEntry &entry);

	QVector<AuthListEntry> m_list;
	bool m_isOperator = false;
	QString m_ownAuthId;
};

}

#endif
