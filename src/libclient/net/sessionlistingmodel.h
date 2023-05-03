// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SESSIONLISTINGMODEL_H
#define SESSIONLISTINGMODEL_H

#include "libshared/listings/announcementapi.h"

#include <QAbstractItemModel>
#include <QUrl>

/**
 * @brief List of sessions received from a listing server
 */
class SessionListingModel final : public QAbstractItemModel
{
	Q_OBJECT
public:
	enum SessionListingRoles {
		SortKeyRole = Qt::UserRole,
		UrlRole,
		IsPasswordedRole,
		IsClosedRole,
		IsNsfwRole
	};

	enum Column : int {
		ColumnVersion,
		ColumnStatus,
		ColumnTitle,
		ColumnServer,
		ColumnUsers,
		ColumnOwner,
		ColumnUptime,
		ColumnCount,
	};

	SessionListingModel(QObject *parent=nullptr);

	QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex &child) const override;
	int rowCount(const QModelIndex &parent=QModelIndex()) const override;
	int columnCount(const QModelIndex &parent=QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role=Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;

	QModelIndex indexOfListing(const QString &listing) const;

public slots:
	void setMessage(const QString &name, const QString &message);
	void setList(const QString &name, const QVector<sessionlisting::Session> sessions);

private:
	struct Listing {
		QString name;

		// If a message is set, the session list is not shown
		QString message;
		QVector<sessionlisting::Session> sessions;
	};
	QVector<Listing> m_listings;
};

#endif

