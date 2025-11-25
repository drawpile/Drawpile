// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_TOUCH_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_TOUCH_H
#include "desktop/dialogs/settingsdialog/page.h"

class QDialogButtonBox;
class QFormLayout;
class QPushButton;

namespace dialogs {
namespace settingsdialog {

class Touch final : public Page {
	Q_OBJECT
public:
	Touch(config::Config *cfg, QWidget *parent = nullptr);

	void createButtons(QDialogButtonBox *buttons);
	void showButtons();

	static void
	addTouchPressureSettingTo(config::Config *cfg, QFormLayout *form);

signals:
	void touchTesterRequested();

protected:
	void setUp(config::Config *cfg, QVBoxLayout *layout) override;

private:
	void initMode(config::Config *cfg, QFormLayout *form);

	void initTapActions(config::Config *cfg, QFormLayout *form);

	void initTapAndHoldActions(config::Config *cfg, QFormLayout *form);

	void initTouchActions(config::Config *cfg, QFormLayout *form);

	QPushButton *m_touchTesterButton;
};

}
}

#endif
