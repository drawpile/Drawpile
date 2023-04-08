// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AUTHDIALOG_H
#define AUTHDIALOG_H

#include <QDialog>

class Ui_AuthDialog;

namespace server {
namespace gui {

class AuthDialog final : public QDialog
{
	Q_OBJECT
public:
	explicit AuthDialog(QWidget *parent=nullptr);
	~AuthDialog() override;

	static void init();

private:
	Ui_AuthDialog *m_ui;
};

}
}

#endif
