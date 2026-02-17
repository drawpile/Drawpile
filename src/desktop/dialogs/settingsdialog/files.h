// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_FILES_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_FILES_H
#include "desktop/dialogs/settingsdialog/page.h"

class QAction;
class QCheckBox;
class QFormLayout;

namespace dialogs {
namespace settingsdialog {

class Files final : public Page {
	Q_OBJECT
public:
	Files(
		config::Config *cfg, QAction *autorecordAction,
		QWidget *parent = nullptr);

protected:
	void setUp(config::Config *cfg, QVBoxLayout *layout) override;

private:
	void initFormats(config::Config *cfg, QFormLayout *form);
	void initAutorecord(config::Config *cfg, QFormLayout *form);
	void initDialogs(config::Config *cfg, QFormLayout *form);
	void initLogging(config::Config *cfg, QFormLayout *form);

	void updateAutoRecordCurrent();

	QAction *m_autorecordAction;
	QCheckBox *m_autoRecordCurrent;
};

}
}

#endif
