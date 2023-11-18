// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_PAGE_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_PAGE_H
#include <QScrollArea>

class QVBoxLayout;

namespace desktop {
namespace settings {
class Settings;
}
}

namespace dialogs {
namespace settingsdialog {

class Page : public QScrollArea {
protected:
	Page(QWidget *parent);

	void init(desktop::settings::Settings &settings, bool stretch = true);

	virtual void
	setUp(desktop::settings::Settings &settings, QVBoxLayout *layout) = 0;
};


// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69210
namespace diagnostic_marker_private {
class [[maybe_unused]] PageMarker : Page {
	void setUp(desktop::settings::Settings &, QVBoxLayout *) override {}
};
}

}
}

#endif
