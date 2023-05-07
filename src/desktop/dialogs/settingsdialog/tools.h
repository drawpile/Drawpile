// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_TOOLS_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_TOOLS_H

#include <QWidget>

namespace desktop { namespace settings { class Settings; } }
namespace utils { class SanerFormLayout; }

namespace dialogs {
namespace settingsdialog {

class Tools final : public QWidget {
	Q_OBJECT
public:
	Tools(desktop::settings::Settings &settings, QWidget *parent = nullptr);
private:
	void initColorWheel(desktop::settings::Settings &settings, utils::SanerFormLayout *layout);
	void initGeneralTools(desktop::settings::Settings &settings, utils::SanerFormLayout *layout);
};

} // namespace settingsdialog
} // namespace dialogs

#endif
