// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_USERINTERFACE_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_USERINTERFACE_H
#include <QWidget>

namespace desktop {
namespace settings {
class Settings;
}
}
namespace utils {
class SanerFormLayout;
}

namespace dialogs {
namespace settingsdialog {

class UserInterface final : public QWidget {
	Q_OBJECT
public:
	UserInterface(
		desktop::settings::Settings &settings, QWidget *parent = nullptr);

private:
	void initFontSize(
		desktop::settings::Settings &settings, utils::SanerFormLayout *form);
	void initInterfaceMode(
		desktop::settings::Settings &settings, utils::SanerFormLayout *form);
	void initMiscellaneous(
		desktop::settings::Settings &settings, utils::SanerFormLayout *form);
};

}
}

#endif
