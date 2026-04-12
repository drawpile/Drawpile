// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_STARTDIALOG_RECOVERNOTICE_H
#define DESKTOP_DIALOGS_STARTDIALOG_RECOVERNOTICE_H
#include <QWidget>

namespace dialogs {
namespace startdialog {

class RecoverNotice final : public QWidget {
	Q_OBJECT
public:
	RecoverNotice(QWidget *parent = nullptr);

Q_SIGNALS:
	void switchToRecoverPageRequested();
};

}
}

#endif
