// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_NOTIFICATIONS_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_NOTIFICATIONS_H

#include <QWidget>

namespace desktop { namespace settings { class Settings; }}
namespace utils { class SanerFormLayout; }

namespace dialogs {
namespace settingsdialog {

class Notifications final : public QWidget {
	Q_OBJECT
public:
	Notifications(desktop::settings::Settings &settings, QWidget *parent = nullptr);
private:
	void initSounds(desktop::settings::Settings &settings, utils::SanerFormLayout *form);
};

} // namespace settingsdialog
} // namespace dialogs

#endif
