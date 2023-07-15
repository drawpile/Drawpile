// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_STARTDIALOG_WELCOME_H
#define DESKTOP_DIALOGS_STARTDIALOG_WELCOME_H

#include "desktop/dialogs/startdialog/page.h"
#include <QWidget>

namespace dialogs {
namespace startdialog {

class Welcome final : public Page {
	Q_OBJECT
public:
	Welcome(QWidget *parent = nullptr);
	void activate() override;

signals:
	void showButtons();
};

}
}

#endif
