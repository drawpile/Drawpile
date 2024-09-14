// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_TOOLS_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_TOOLS_H
#include "desktop/dialogs/settingsdialog/page.h"

class QFormLayout;

namespace desktop {
namespace settings {
class Settings;
}
}

namespace dialogs {
namespace settingsdialog {

class Tools final : public Page {
	Q_OBJECT
public:
	Tools(desktop::settings::Settings &settings, QWidget *parent = nullptr);

protected:
	void
	setUp(desktop::settings::Settings &settings, QVBoxLayout *layout) override;

private:
	void
	initColorSpace(desktop::settings::Settings &settings, QFormLayout *form);

	void
	initCursors(desktop::settings::Settings &settings, QFormLayout *form);

	void initKeyboardShortcuts(
		desktop::settings::Settings &settings, QFormLayout *form);

	void
	initSlots(desktop::settings::Settings &settings, QFormLayout *form);
};

} // namespace settingsdialog
} // namespace dialogs

#endif
