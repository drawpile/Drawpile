// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_CHAT_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_CHAT_H
#include "desktop/dialogs/settingsdialog/page.h"

class QFormLayout;

namespace dialogs {
namespace settingsdialog {

class Network final : public Page {
	Q_OBJECT
public:
	Network(config::Config *cfg, QWidget *parent = nullptr);

protected:
	void setUp(config::Config *cfg, QVBoxLayout *layout) override;

private:
	void initAvatars(QVBoxLayout *layout);

	void initBuiltinServer(config::Config *cfg, QFormLayout *form);

	void initNetwork(config::Config *cfg, QFormLayout *form);
};

}
}

#endif
