// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_USERINTERFACE_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_USERINTERFACE_H
#include "desktop/dialogs/settingsdialog/page.h"

class QFormLayout;
class QVBoxLayout;

namespace dialogs {
namespace settingsdialog {

class UserInterface final : public Page {
	Q_OBJECT
public:
	UserInterface(config::Config *cfg, QWidget *parent = nullptr);

Q_SIGNALS:
	void scalingChangeRequested();

protected:
	void setUp(config::Config *cfg, QVBoxLayout *layout) override;

private:
	void initInterfaceMode(config::Config *cfg, QFormLayout *form);

	void initKineticScrolling(config::Config *cfg, QFormLayout *form);

	void initMiscellaneous(config::Config *cfg, QFormLayout *form);

	void initRequiringRestart(config::Config *cfg, QFormLayout *form);

	void pickColor(
		config::Config *cfg, QColor (config::Config::*getColor)() const,
		void (config::Config::*setColor)(const QColor &),
		const QColor &defaultColor);
};

}
}

#endif
