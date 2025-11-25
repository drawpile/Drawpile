// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_PAGE_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_PAGE_H
#include <QScrollArea>

class QVBoxLayout;

namespace config {
class Config;
}

namespace dialogs {
namespace settingsdialog {

class Page : public QScrollArea {
protected:
	Page(QWidget *parent);

	void init(config::Config *cfg, bool stretch = true);

	virtual void setUp(config::Config *cfg, QVBoxLayout *layout) = 0;

	void disableKineticScrollingOnWidget(QWidget *widget);
};

}
}

#endif
