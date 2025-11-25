// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_TABLET_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_TABLET_H
#include "desktop/dialogs/settingsdialog/page.h"

class QDialogButtonBox;
class QPushButton;
class QFormLayout;

namespace dialogs {
namespace settingsdialog {

class Tablet final : public Page {
	Q_OBJECT
public:
	Tablet(config::Config *cfg, QWidget *parent = nullptr);

	void createButtons(QDialogButtonBox *buttons);
	void showButtons();

signals:
	void tabletTesterRequested();

protected:
	void
	setUp(config::Config *cfg, QVBoxLayout *layout) override;

private:
	void
	initPressureCurve(config::Config *cfg, QFormLayout *form);

	void initTablet(config::Config *cfg, QFormLayout *form);

	QPushButton *m_tabletTesterButton = nullptr;
};

}
}

#endif
