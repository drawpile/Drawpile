// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_INVITELISTMODEL_H
#define LIBCLIENT_UTILS_INVITELISTMODEL_H
#include <QAbstractItemModel>
#include <QDateTime>
#include <QSize>
#include <QString>
#include <QVector>

class QJsonArray;

namespace net {

class InviteListModel final : public QAbstractItemModel {
	Q_OBJECT
public:
	struct Use {
		QString name;
		QDateTime at;

		static Use fromJson(const QJsonObject &json);

		bool operator==(const Use &other) const;
		bool operator<(const Use &other) const;
	};

	struct Invite {
		QString secret;
		QString creator;
		QDateTime at;
		QVector<Use> uses;
		int maxUses;
		bool op;
		bool trust;

		static Invite fromJson(const QJsonObject &json);
		static QVector<Use> usesFromJson(const QJsonArray &json);

		bool operator<(const Invite &other) const;
	};

	enum class Column { Secret, Creator, Role, Uses, Count };
	enum Role { SecretRole = Qt::UserRole };

	explicit InviteListModel(QObject *parent = nullptr);

	QModelIndex index(
		int row, int column,
		const QModelIndex &parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex &child) const override;
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;

	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

	QVariant headerData(
		int section, Qt::Orientation orientation,
		int role = Qt::DisplayRole) const override;

	Qt::ItemFlags flags(const QModelIndex &index) const override;
	QSize span(const QModelIndex &index) const override;

	bool isEmpty() const { return m_invites.isEmpty(); }
	void update(const QJsonArray json);
	void clear();

	QModelIndex indexOfSecret(const QString &secret) const;
	QStringList getSecretsInOrder(const QSet<QString> &secrets) const;

signals:
	void invitesUpdated();

private:
	bool isInBounds(int row, int column, const QModelIndex &parent) const;
	bool isIndexInBounds(const QModelIndex &index) const;

	QVector<Invite> m_invites;
};

}

#endif
