// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_TABLET_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_TABLET_H
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

class Tablet final : public Page {
	Q_OBJECT
public:
	Tablet(desktop::settings::Settings &settings, QWidget *parent = nullptr);

	void createButtons(QDialogButtonBox *buttons);
	void showButtons();

signals:
	void tabletTesterRequested();

protected:
	void
	setUp(desktop::settings::Settings &settings, QVBoxLayout *layout) override;

private:
	void
	initPressureCurve(desktop::settings::Settings &settings, QFormLayout *form);

	void initTablet(desktop::settings::Settings &settings, QFormLayout *form);

	QPushButton *m_tabletTesterButton = nullptr;
};

} // namespace settingsdialog
} // namespace dialogs

#endif
