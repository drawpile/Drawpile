// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/tablettester.h"
#include "desktop/main.h"
#include "ui_tablettest.h"
#include <QPushButton>

namespace dialogs {

TabletTestDialog::TabletTestDialog(QWidget *parent)
	: QDialog(parent)
{
	m_ui = new Ui_TabletTest;
	m_ui->setupUi(this);

	connect(
		m_ui->buttons->button(QDialogButtonBox::Reset),
		&QAbstractButton::clicked, m_ui->tablettest,
		&widgets::TabletTester::clear);
	connect(
		m_ui->tablettest, &widgets::TabletTester::eventReport, m_ui->logView,
		&QPlainTextEdit::appendPlainText);
	connect(
		m_ui->buttons->button(QDialogButtonBox::Reset),
		&QAbstractButton::clicked, m_ui->logView, &QPlainTextEdit::clear);
	connect(m_ui->buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(&dpApp(), &DrawpileApp::eraserNear, this, [this](bool near) {
		QString msg;
		if(near) {
			msg = QStringLiteral("Eraser brought near");
		} else {
			msg = QStringLiteral("Eraser taken away");
		}
		m_ui->logView->appendPlainText(msg);
	});
}

TabletTestDialog::~TabletTestDialog()
{
	delete m_ui;
}

}
