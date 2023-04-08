// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/tablettester.h"
#include "ui_tablettest.h"

#include "desktop/main.h"

namespace dialogs {

TabletTestDialog::TabletTestDialog( QWidget *parent) :
	QDialog(parent)
{
	m_ui = new Ui_TabletTest;
	m_ui->setupUi(this);

	connect(static_cast<DrawpileApp*>(qApp), &DrawpileApp::eraserNear, this, [this](bool near) {
		QString msg;
		if(near)
			msg = QStringLiteral("Eraser brought near");
		else
			msg = QStringLiteral("Eraser taken away");

		m_ui->logView->appendPlainText(msg);
	});
}

TabletTestDialog::~TabletTestDialog()
{
	delete m_ui;
}

}

