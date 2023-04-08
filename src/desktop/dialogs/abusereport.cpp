// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/abusereport.h"
#include "desktop/utils/mandatoryfields.h"

#include "ui_abusereport.h"

#include <QPushButton>

namespace dialogs {

AbuseReportDialog::AbuseReportDialog(QWidget *parent)
	: QDialog(parent), m_sessionId(QString()), m_userId(0)
{
	m_ui = new Ui_AbuseReportDialog;
	m_ui->setupUi(this);
	m_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Report"));

	new MandatoryFields(this, m_ui->buttons->button(QDialogButtonBox::Ok));
}

AbuseReportDialog::~AbuseReportDialog()
{
	delete m_ui;
}

void AbuseReportDialog::setSessionInfo(const QString &id, const QString &alias, const QString &title)
{
	m_ui->sessionTitle->setText(title);
	if(alias.isEmpty()) {
		m_ui->sessionId->setText(id);
	} else {
		m_ui->sessionId->setText(QString("(%1 / %2)").arg(alias, id));
	}
	m_sessionId = id;
}

void AbuseReportDialog::addUser(int id, const QString &name)
{
	m_ui->user->setEnabled(true);
	m_ui->user->addItem(name, id);
}

QString AbuseReportDialog::message() const
{
	return m_ui->reason->text();
}

int AbuseReportDialog::userId() const
{
	bool ok;
	int id = m_ui->user->currentData().toInt(&ok);
	if(!ok)
		return 0;
	return id;
}

}

