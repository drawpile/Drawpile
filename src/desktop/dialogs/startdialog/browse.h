// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_STARTDIALOG_BROWSE_H
#define DESKTOP_DIALOGS_STARTDIALOG_BROWSE_H

#include "desktop/dialogs/startdialog/page.h"
#include <QHash>
#include <QWidget>

class QAction;
class QCheckBox;
class QIcon;
class QLineEdit;
class QMenu;
class QTimer;
class QUrl;
class ListingSessionFilterProxyModel;
class SessionListingModel;

namespace sessionlisting {
class AnnouncementApiResponse;
struct ListServer;
}

namespace widgets {
class SpanAwareTreeView;
}

namespace dialogs {
namespace startdialog {

class Browse final : public Page {
	Q_OBJECT
public:
	Browse(QWidget *parent = nullptr);
	void activate() final override;
	void accept() final override;

signals:
	void hideLinks();
	void showButtons();
	void enableJoin(bool enabled);
	void join(const QUrl &url);
	void addListServerUrlRequested(const QUrl &url);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
	void updateListServers(const QVector<QVariantMap> &settingsListServers);
	void periodicRefresh();
	void showListingContextMenu(const QPoint &pos);
	void joinIndex(const QModelIndex &index);
    void cascadeSectionResize(int logicalIndex, int oldSize, int newSize);

private:
	static constexpr int REFRESH_INTERVAL_SECS = 60;

	void refresh();
	void refreshServer(const sessionlisting::ListServer &ls, const QUrl &url);

	void updateJoinButton();

	QAction *makeCopySessionDataAction(const QString &text, int role);

	bool canJoinIndex(const QModelIndex &index);
	bool isListingIndex(const QModelIndex &index);

	QWidget *m_noListServers;
	QLineEdit *m_filterEdit;
	QCheckBox *m_closedBox;
	QCheckBox *m_passwordBox;
	QCheckBox *m_nsfmBox;
	QCheckBox *m_inactiveBox;
	QCheckBox *m_duplicatesBox;
	widgets::SpanAwareTreeView *m_listing;
	SessionListingModel *m_sessions;
	ListingSessionFilterProxyModel *m_filteredSessions;
	QMenu *m_listingContextMenu;
	QAction *m_joinAction;
	QTimer *m_refreshTimer;
	qint64 m_lastRefresh = 0;
	QHash<QString, sessionlisting::AnnouncementApiResponse *>
		m_refreshesInProgress;
	bool m_sectionFitInProgress = false;
	bool m_activated = false;
};

}
}

#endif
