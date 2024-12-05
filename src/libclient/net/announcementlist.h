// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_NET_ANNOUNCEMENTLIST_H
#define LIBCLIENT_NET_ANNOUNCEMENTLIST_H
#include <QAbstractTableModel>
#include <QIcon>
#include <QStringList>
#include <QVector>

namespace net {

/**
 * A list model to represent active session announcements.
 */
class AnnouncementListModel final : public QAbstractTableModel {
	Q_OBJECT
public:
	explicit AnnouncementListModel(QObject *parent = nullptr);
	explicit AnnouncementListModel(
		const QVector<QVariantMap> &data, QObject *parent = nullptr);

	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;

	const QStringList &announcements() const { return m_announcements; }

	void setAnnouncements(const QStringList &urls);

	/**
	 * @brief Add or update a listed announcement
	 *
	 * If an announcement with the same URL already exists in the list, it is
	 * updated in place.
	 */
	void addAnnouncement(const QString &url);

	/**
	 * @brief Remove an announcement from the list
	 *
	 * The announcement with the given server URL will be removed (if it is
	 * listed.)
	 */
	void removeAnnouncement(const QString &url);

	//! Clear the whole list
	void clear();

	void setKnownServers(const QVector<QVariantMap> &data);

private:
	void loadKnownServers(const QVector<QVariantMap> &data);

	QStringList m_announcements;
	QHash<QString, QPair<QIcon, QString>> m_knownServers;
};

}

#endif
