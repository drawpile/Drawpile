// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_NOTIFICATIONS_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_NOTIFICATIONS_H
#include "desktop/dialogs/settingsdialog/page.h"

class QFormLayout;

namespace dialogs {
namespace settingsdialog {

class Notifications final : public Page {
	Q_OBJECT
public:
	Notifications(config::Config *cfg, QWidget *parent = nullptr);

protected:
	void setUp(config::Config *cfg, QVBoxLayout *layout) override;

private:
#ifdef Q_OS_ANDROID
	void initAndroid(config::Config *cfg, QFormLayout *form);
#endif
	void initGrid(config::Config *cfg, QVBoxLayout *layout);
	void initOptions(config::Config *cfg, QFormLayout *form);
};

}
}

#endif
