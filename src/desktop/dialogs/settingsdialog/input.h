// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_INPUT_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_INPUT_H
#include "desktop/dialogs/settingsdialog/page.h"

class QFormLayout;

namespace desktop {
namespace settings {
class Settings;
}
}

namespace dialogs {
namespace settingsdialog {

class Input final : public Page {
	Q_OBJECT
public:
	Input(desktop::settings::Settings &settings, QWidget *parent = nullptr);

protected:
	void
	setUp(desktop::settings::Settings &settings, QVBoxLayout *layout) override;

private:
#ifdef Q_OS_ANDROID
	void initAndroid(desktop::settings::Settings &settings, QFormLayout *form);
#endif

	void
	initPressureCurve(desktop::settings::Settings &settings, QFormLayout *form);

	void initTablet(desktop::settings::Settings &settings, QVBoxLayout *layout);
};

} // namespace settingsdialog
} // namespace dialogs

#endif
