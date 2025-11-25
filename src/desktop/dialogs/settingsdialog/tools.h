// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_TOOLS_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_TOOLS_H
#include "desktop/dialogs/settingsdialog/page.h"

class QFormLayout;

namespace dialogs {
namespace settingsdialog {

class Tools final : public Page {
	Q_OBJECT
public:
	Tools(config::Config *cfg, QWidget *parent = nullptr);

protected:
	void setUp(config::Config *cfg, QVBoxLayout *layout) override;

private:
	void initColorSpace(config::Config *cfg, QFormLayout *form);

	void initCursors(config::Config *cfg, QFormLayout *form);

	void initKeyboardShortcuts(config::Config *cfg, QFormLayout *form);

	void initSlots(config::Config *cfg, QFormLayout *form);
};

}
}

#endif
