// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_FILES_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_FILES_H
#include "desktop/dialogs/settingsdialog/page.h"

class QDialogButtonBox;
class QPushButton;
class QFormLayout;

namespace desktop {
namespace settings {
class Settings;
}
}

namespace dialogs {
namespace settingsdialog {

class Files final : public Page {
	Q_OBJECT
public:
	Files(desktop::settings::Settings &settings, QWidget *parent = nullptr);

protected:
	void
	setUp(desktop::settings::Settings &settings, QVBoxLayout *layout) override;

private:
	void initFormats(desktop::settings::Settings &settings, QFormLayout *form);
	void initAutosave(desktop::settings::Settings &settings, QFormLayout *form);
};

}
}

#endif
