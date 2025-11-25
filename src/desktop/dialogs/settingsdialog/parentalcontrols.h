// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_CONTENTFILTER_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_CONTENTFILTER_H
#include "desktop/dialogs/settingsdialog/page.h"

class QVBoxLayout;

namespace dialogs {
namespace settingsdialog {

class ParentalControls final : public Page {
	Q_OBJECT
public:
	ParentalControls(config::Config *cfg, QWidget *parent = nullptr);

protected:
	void setUp(config::Config *cfg, QVBoxLayout *layout) override;

private:
	void initBuiltIn(config::Config *cfg, QVBoxLayout *layout);
	void initInfoBar(QVBoxLayout *layout);
	void initOsManaged(QVBoxLayout *layout);
	void toggleLock();

	config::Config *m_cfg;
};

}
}

#endif
