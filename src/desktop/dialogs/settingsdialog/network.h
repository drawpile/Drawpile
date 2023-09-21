// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_CHAT_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_CHAT_H

#include <QWidget>

namespace desktop { namespace settings { class Settings; }}
namespace utils { class SanerFormLayout; }

namespace dialogs {
namespace settingsdialog {

class Network final : public QWidget {
	Q_OBJECT
public:
	Network(desktop::settings::Settings &settings, QWidget *parent = nullptr);
private:
	void initAvatars(utils::SanerFormLayout *form);
	void initBuiltinServer(desktop::settings::Settings &settings, utils::SanerFormLayout *form);
};

} // namespace settingsdialog
} // namespace dialogs

#endif
