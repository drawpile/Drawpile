// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_STARTDIALOG_RECENT_H
#define DESKTOP_DIALOGS_STARTDIALOG_RECENT_H

#include "desktop/dialogs/startdialog/page.h"

namespace widgets {
class RecentScroll;
}

namespace dialogs {
namespace startdialog {

class Recent final : public Page {
	Q_OBJECT
public:
	Recent(QWidget *parent = nullptr);

signals:
	void openUrl(const QUrl &url);

private slots:
	void recentFileSelected(const QString &path);

private:
	widgets::RecentScroll *m_recentScroll;
};

}
}

#endif
