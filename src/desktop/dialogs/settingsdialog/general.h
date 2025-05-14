// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_GENERAL_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_GENERAL_H
#include "desktop/dialogs/settingsdialog/page.h"

class QFormLayout;
class QLocale;

namespace desktop {
namespace settings {
class Settings;
}
}

namespace dialogs {
namespace settingsdialog {

class General final : public Page {
	Q_OBJECT
public:
	General(desktop::settings::Settings &settings, QWidget *parent = nullptr);

protected:
	void
	setUp(desktop::settings::Settings &settings, QVBoxLayout *layout) override;

private:
	void initLanguage(desktop::settings::Settings &settings, QFormLayout *form);

	static QString formatLanguage(const QLocale &locale);

	void initLogging(desktop::settings::Settings &settings, QFormLayout *form);

	void
	initPerformance(desktop::settings::Settings &settings, QFormLayout *form);

	void
	initSnapshots(desktop::settings::Settings &settings, QFormLayout *form);

	void initTheme(desktop::settings::Settings &settings, QFormLayout *form);

	void initUndo(desktop::settings::Settings &settings, QFormLayout *form);
};

} // namespace settingsdialog
} // namespace dialogs

#endif
