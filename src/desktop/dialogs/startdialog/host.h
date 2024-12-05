// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_STARTDIALOG_HOST_H
#define DESKTOP_DIALOGS_STARTDIALOG_HOST_H
#include "desktop/dialogs/startdialog/page.h"
#include "desktop/utils/hostparams.h"

class QJsonObject;
class QStackedWidget;
class QTabBar;

namespace dialogs {
namespace startdialog {

namespace host {
class Bans;
class Listing;
class Permissions;
class Roles;
class Session;
}

class Host final : public Page {
	Q_OBJECT
public:
	Host(QWidget *parent = nullptr);
	void activate() override;
	void accept() override;
	void triggerReset();
	void triggerLoad();
	void triggerSave();
	void triggerImport();
	void triggerExport();

signals:
	void hideLinks();
	void showButtons();
	void host(const HostParams &params);
	void switchToJoinPageRequested();

private:
	void startEditingTitle();
	void loadCategory(
		int category, const QJsonObject &json, bool replaceAnnouncements,
		bool replaceAuth, bool replaceBans);
	void resetCategory(
		int category, bool replaceAnnouncements, bool replaceAuth,
		bool replaceBans);
	void onImportFinished(const QJsonObject &json);
	void onImportFailed(const QString &error);

	QTabBar *m_tabs;
	QStackedWidget *m_stack;
	host::Session *m_sessionPage;
	host::Listing *m_listingPage;
	host::Permissions *m_permissionsPage;
	host::Roles *m_rolesPage;
	host::Bans *m_bansPage;
};

}
}

#endif
