// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_GENERAL_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_GENERAL_H
#include "desktop/dialogs/settingsdialog/page.h"

class QFormLayout;
class QLocale;

namespace dialogs {
namespace settingsdialog {

class General final : public Page {
	Q_OBJECT
public:
	General(config::Config *cfg, QWidget *parent = nullptr);

protected:
	void setUp(config::Config *cfg, QVBoxLayout *layout) override;

private:
	void initLanguage(config::Config *cfg, QFormLayout *form);

	static QString formatLanguage(const QLocale &locale);

	void initContributing(config::Config *cfg, QFormLayout *form);

	void initPerformance(config::Config *cfg, QFormLayout *form);

	void initSnapshots(config::Config *cfg, QFormLayout *form);

	void initTheme(config::Config *cfg, QFormLayout *form);

	void initUndo(config::Config *cfg, QFormLayout *form);
};

}
}

#endif
