// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LISTSERVERMODEL_H
#define LISTSERVERMODEL_H

#include <QAbstractListModel>
#include <QIcon>

namespace libclient { namespace settings { class Settings; } }

namespace sessionlisting {

struct ListServer {
	QIcon icon;
	QString iconName;
	QString name;
	QString url;
	QString description;
	bool readonly;
	bool publicListings;
	bool privateListings;
};

class ListServerModel final : public QAbstractListModel
{
	Q_OBJECT
public:
	enum ListServeroles {
		UrlRole = Qt::UserRole,
		DescriptionRole,
		PublicListRole,
		PrivateListRole
	};

	explicit ListServerModel(libclient::settings::Settings &settings, bool includeReadOnly, QObject *parent=nullptr);

	int rowCount(const QModelIndex &parent=QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const override;

	bool removeRows(int row, int count, const QModelIndex &parent) override;

	bool moveServer(int sourceRow, int destinationChild);

	/**
	 * @brief Add a new server to the list
	 * @return false if an existing item was updated instead of added
	 */
	bool addServer(const QString &name, const QString &url, const QString &description, bool readonly, bool pub, bool priv);

	/**
	 * @brief Remove the server with the given URL
	 * @return false if no such server was found
	 */
	bool removeServer(const QString &url);

	//!  Set the favicon for the server with the given URL
	QIcon setFavicon(const QString &url, const QImage &icon);

	//! Get all configured list servers
	static QVector<ListServer> listServers(const QVector<QVariantMap> &cfg, bool includeReadOnly);

private:
	//! Load server list from the settings file
	void loadServers(const QVector<QVariantMap> &cfg, bool includeReadOnly);

	//! Save (modified) server list. This replaces the existing list
	[[nodiscard]] QVector<QVariantMap> saveServers() const;

public slots:
	void revert() override;
	bool submit() override;

private:
	libclient::settings::Settings &m_settings;
	bool m_includeReadOnly;
	QVector<ListServer> m_servers;
};

}

#endif // LISTSERVERMODEL_H
