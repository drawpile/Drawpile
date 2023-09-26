// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_CHAT_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_CHAT_H
#include "desktop/dialogs/settingsdialog/page.h"

class QFormLayout;

namespace desktop {
namespace settings {
class Settings;
}
}

namespace dialogs {
namespace settingsdialog {

class Network final : public Page {
	Q_OBJECT
public:
	Network(desktop::settings::Settings &settings, QWidget *parent = nullptr);

protected:
	void
	setUp(desktop::settings::Settings &settings, QVBoxLayout *layout) override;

private:
	void initAvatars(QVBoxLayout *layout);

	void
	initBuiltinServer(desktop::settings::Settings &settings, QFormLayout *form);

	void initNetwork(desktop::settings::Settings &settings, QFormLayout *form);
};

} // namespace settingsdialog
} // namespace dialogs

#endif
