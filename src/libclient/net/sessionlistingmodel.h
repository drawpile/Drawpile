// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SESSIONLISTINGMODEL_H
#define SESSIONLISTINGMODEL_H

#include "libshared/listings/announcementapi.h"

#include <QAbstractItemModel>
#include <QHash>
#include <QImage>
#include <QUrl>

/**
 * @brief List of sessions received from a listing server
 */
class SessionListingModel final : public QAbstractItemModel
{
	Q_OBJECT
public:
	enum Column {
		Title,
		Message = Title,
		Server,
		UserCount,
		Owner,
		Uptime,
		Version,
		ColumnCount
	};

	enum SessionListingRoles {
		SortKeyRole = Qt::UserRole,
		UrlRole,
		IsPasswordedRole,
		IsClosedRole,
		IsNsfwRole,
		UrlStringRole,
		IsListingRole,
		TitleRole,
		ServerRole,
		OwnerRole,
	};

	SessionListingModel(QObject *parent=nullptr);

	QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex &child) const override;
	int rowCount(const QModelIndex &parent=QModelIndex()) const override;
	int columnCount(const QModelIndex &parent=QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role=Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;
	QSize span(const QModelIndex &index) const override;

	QModelIndex indexOfListing(const QString &listing) const;

	QModelIndex primaryIndexOfUrl(const QUrl &url) const;

public slots:
	void setIcon(const QString &name, const QIcon &icon);
	void setMessage(const QString &name, const QString &host, const QString &message);
	void setList(const QString &name, const QString &host, const QVector<sessionlisting::Session> sessions);
	void clear();

private:
	QString formatTitle(
		const sessionlisting::Session &session, bool includeFlags) const;

	static QString squashWhitespace(const QString &s);

	bool isNsfm(const sessionlisting::Session &session) const;
	bool isClosed(const sessionlisting::Session &session) const;

	inline bool isRootItem(const QModelIndex &index) const { return index.internalId() == 0; }
	inline int listingIndex(const QModelIndex &index) const {
		return isRootItem(index) ? index.row() : int(index.internalId() - 1);
	}

	struct Listing {
		QString name;
		QString host;

		// If a message is set, the session list is not shown
		QString message;
		QVector<sessionlisting::Session> sessions;

		inline bool offline() const { return !message.isEmpty(); }
	};

	QVector<Listing> m_listings;
	QHash<QString, QIcon> m_icons;
};

#endif
