/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "versioncheckdialog.h"

#include "widgets/spinner.h"
using widgets::Spinner;

#include "ui_versioncheck.h"

#include <QSettings>

namespace dialogs {

VersionCheckDialog::VersionCheckDialog(QWidget *parent)
	: QDialog(parent), m_ui(new Ui_VersionCheckDialog)
{
	m_ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose);

	m_ui->dontCheck->setChecked(!QSettings().value("versioncheck/enabled", true).toBool());

	connect(this, &VersionCheckDialog::finished, this, &VersionCheckDialog::rememberSettings);
}

VersionCheckDialog::~VersionCheckDialog()
{
	delete m_ui;
}

void VersionCheckDialog::rememberSettings()
{
	QSettings().setValue("versioncheck/enabled", !m_ui->dontCheck->isChecked());
}

void VersionCheckDialog::doVersionCheckIfNeeded()
{
	if(NewVersionCheck::needCheck()) {
		// The dialog will autodelete if there is nothing to show
		VersionCheckDialog *dlg = new VersionCheckDialog;
		dlg->queryNewVersions();
	}
}

void VersionCheckDialog::queryNewVersions()
{
	m_newversion = new NewVersionCheck(this);
	connect(m_newversion, &NewVersionCheck::versionChecked, this, &VersionCheckDialog::versionChecked);
	m_newversion->queryVersions();
}

void VersionCheckDialog::versionChecked(bool isNew, const QString &errorMessage)
{
	if(!isNew && !isVisible()) {
		// If dialog is not yet visible, this was a background version check.
		// Don't bother the user if there is nothing to tell.
		deleteLater();
		return;
	}

	if(!errorMessage.isEmpty()) {
		m_ui->textBrowser->insertHtml(QStringLiteral("<p style=\"color: red\">%1</p>").arg(errorMessage));
		m_ui->views->setCurrentIndex(1);

	} else {
		setNewVersions(m_newversion->getNewer());
	}

	show();
}

void VersionCheckDialog::setNewVersions(const QVector<NewVersionCheck::Version> &versions)
{
	if(versions.isEmpty()) {
		m_ui->textBrowser->insertHtml("<h1>You're up to date!</h1><p>No new versions found.</p>");

	} else {
		QTextCursor cursor(m_ui->textBrowser->document());

		cursor.insertHtml("<h1>A new version of Drawpile is available!</h1>");
		cursor.insertBlock();

		for(const auto &version : versions) {
			cursor.insertHtml(QStringLiteral("<h2><a href=\"%1\">Version %2</a></h2>").arg(version.announcementUrl, version.version));
			cursor.insertBlock();
			cursor.insertHtml(version.description);
		}
	}
	m_ui->views->setCurrentIndex(1);
}

}
