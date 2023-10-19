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

	const QVector<AuthListEntry> &entries() const { return m_list; }

	void update(const QJsonArray &auth);

	void clear();

	void setIsOperator(bool isOperator);

	void setOwnAuthId(const QString &authId);

signals:
	void updateApplied();

private:
	QVector<AuthListEntry> m_list;
	bool m_isOperator = false;
	QString m_ownAuthId;
};

}

#endif
