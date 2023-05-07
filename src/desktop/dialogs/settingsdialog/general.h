// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_GENERAL_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_GENERAL_H

#include <QWidget>

namespace desktop { namespace settings { class Settings; }}
namespace utils { class SanerFormLayout; }

namespace dialogs {
namespace settingsdialog {

class General final : public QWidget {
	Q_OBJECT
public:
	General(desktop::settings::Settings &settings, QWidget *parent = nullptr);
private:
	void initLanguage(desktop::settings::Settings &settings, utils::SanerFormLayout *form);
	void initMiscUi(desktop::settings::Settings &settings, utils::SanerFormLayout *form);
	void initSnapshots(desktop::settings::Settings &settings, utils::SanerFormLayout *form);
	void initTheme(desktop::settings::Settings &settings, utils::SanerFormLayout *form);
	void initUndo(desktop::settings::Settings &settings, utils::SanerFormLayout *form);
};

} // namespace settingsdialog
} // namespace dialogs

#endif
