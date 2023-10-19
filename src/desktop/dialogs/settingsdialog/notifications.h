// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_NOTIFICATIONS_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_NOTIFICATIONS_H
#include "desktop/dialogs/settingsdialog/page.h"

class QFormLayout;

namespace desktop {
namespace settings {
class Settings;
}
}

namespace dialogs {
namespace settingsdialog {

class Notifications final : public Page {
	Q_OBJECT
public:
	Notifications(
		desktop::settings::Settings &settings, QWidget *parent = nullptr);

protected:
	void
	setUp(desktop::settings::Settings &settings, QVBoxLayout *layout) override;

private:
	void initGrid(desktop::settings::Settings &settings, QVBoxLayout *layout);
	void initOptions(desktop::settings::Settings &settings, QFormLayout *form);
};

} // namespace settingsdialog
} // namespace dialogs

#endif
