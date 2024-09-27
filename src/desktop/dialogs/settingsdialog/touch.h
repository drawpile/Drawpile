// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_TOUCH_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_TOUCH_H
#include "desktop/dialogs/settingsdialog/page.h"

class QFormLayout;

namespace desktop {
namespace settings {
class Settings;
}
}

namespace dialogs {
namespace settingsdialog {

class Touch final : public Page {
	Q_OBJECT
public:
	Touch(desktop::settings::Settings &settings, QWidget *parent = nullptr);

protected:
	void
	setUp(desktop::settings::Settings &settings, QVBoxLayout *layout) override;

private:
	void initMode(desktop::settings::Settings &settings, QFormLayout *form);

	void
	initTapActions(desktop::settings::Settings &settings, QFormLayout *form);

	void
	initTouchActions(desktop::settings::Settings &settings, QFormLayout *form);
};

}
}

#endif
