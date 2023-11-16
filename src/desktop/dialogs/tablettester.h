// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_TABLETTESTER_H
#define DESKTOP_DIALOGS_TABLETTESTER_H
#include <QDialog>

class Ui_TabletTest;

namespace dialogs {

class TabletTestDialog final : public QDialog {
	Q_OBJECT
public:
	TabletTestDialog(QWidget *parent = nullptr);
	~TabletTestDialog() override;

private:
	Ui_TabletTest *m_ui;
};

}

#endif
