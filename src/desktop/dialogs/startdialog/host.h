// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_STARTDIALOG_HOST_H
#define DESKTOP_DIALOGS_STARTDIALOG_HOST_H
#include "desktop/dialogs/startdialog/page.h"

class QStackedWidget;
class QTabBar;

namespace dialogs {
namespace startdialog {

namespace host {
class Session;
}

class Host final : public Page {
	Q_OBJECT
public:
	Host(QWidget *parent = nullptr);
	void activate() final override;
	void accept() final override;

signals:
	void hideLinks();
	void showButtons();
	void enableHost(bool enabled);
	void host(
		const QString &title, const QString &password, const QString &alias,
		bool nsfm, const QString &announcementUrl,
		const QString &remoteAddress);
	void switchToJoinPageRequested();

private:
	QTabBar *m_tabs;
	QStackedWidget *m_stack;
	host::Session *m_sessionPage;
};

}
}

#endif
