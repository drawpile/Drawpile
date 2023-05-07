// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_INPUT_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_INPUT_H

#include <QWidget>

namespace desktop { namespace settings { class Settings; } }
namespace utils { class SanerFormLayout; }

namespace dialogs {
namespace settingsdialog {

class Input final : public QWidget {
	Q_OBJECT
public:
	Input(desktop::settings::Settings &settings, QWidget *parent = nullptr);
private:
	void initPressureCurve(desktop::settings::Settings &settings, utils::SanerFormLayout *form);
	void initTablet(desktop::settings::Settings &settings, utils::SanerFormLayout *form);
	void initTouch(desktop::settings::Settings &settings, utils::SanerFormLayout *form);
};

} // namespace settingsdialog
} // namespace dialogs

#endif
