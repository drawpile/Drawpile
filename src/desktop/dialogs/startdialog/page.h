// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_STARTDIALOG_PAGE_H
#define DESKTOP_DIALOGS_STARTDIALOG_PAGE_H

#include <QWidget>

namespace dialogs {

class StartDialog;

namespace startdialog {

class Page : public QWidget {
	Q_OBJECT
public:
	Page(QWidget *parent = nullptr);
	virtual void activate();
	virtual void accept();
};

// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69210
namespace diagnostic_marker_private {
class [[maybe_unused]] AbstractPageMarker : Page {
	inline void activate() override {}
	inline void accept() override {}
};
}

}
}

#endif
