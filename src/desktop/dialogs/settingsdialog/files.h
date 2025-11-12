// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_FILES_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_FILES_H
#include "desktop/dialogs/settingsdialog/page.h"

class QDialogButtonBox;
class QPushButton;
class QFormLayout;

namespace dialogs {
namespace settingsdialog {

class Files final : public Page {
	Q_OBJECT
public:
	Files(config::Config *cfg, QWidget *parent = nullptr);

protected:
	void setUp(config::Config *cfg, QVBoxLayout *layout) override;

private:
	void initFormats(config::Config *cfg, QFormLayout *form);
	void initAutorecord(config::Config *cfg, QFormLayout *form);
	void initDialogs(config::Config *cfg, QFormLayout *form);
	void initLogging(config::Config *cfg, QFormLayout *form);
};

}
}

#endif
