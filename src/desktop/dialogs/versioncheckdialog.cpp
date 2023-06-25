// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/versioncheckdialog.h"
#include "desktop/main.h"
#include "libshared/util/networkaccess.h"
#include "libshared/util/paths.h"

#include "ui_versioncheck.h"

#include <QIcon>
#include <QMessageBox>
#include <QStyle>
#include <QPushButton>
#include <QDir>
#include <QDesktopServices>

namespace dialogs {

VersionCheckDialog::VersionCheckDialog(QWidget *parent)
	: QDialog(parent), m_ui(new Ui_VersionCheckDialog)
{
	m_ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose);

	m_downloadButton = m_ui->buttonBox->addButton(tr("Download"), QDialogButtonBox::ActionRole);
	m_downloadButton->setIcon(QIcon::fromTheme("edit-download"));
	m_downloadButton->hide();
	showButtons(QDialogButtonBox::Cancel);

	connect(m_downloadButton, &QPushButton::clicked, this, &VersionCheckDialog::downloadNewVersion);

	dpApp().settings().bindVersionCheckEnabled(m_ui->checkForUpdates, &QCheckBox::setChecked, &QCheckBox::clicked);
}

VersionCheckDialog::~VersionCheckDialog()
{
	delete m_ui;
}

void VersionCheckDialog::doVersionCheckIfNeeded()
{
	if(!dpApp().settings().versionCheckFirstRun()) {
		QMessageBox mb {
			QMessageBox::NoIcon,
			tr("Enable auto-updates?"),
			tr("Should Drawpile automatically check for updates?"),
			QMessageBox::Yes | QMessageBox::No
		};
		const auto icon = QIcon::fromTheme("update-none");
		const auto iconSize = mb.style()->pixelMetric(QStyle::PM_MessageBoxIconSize, nullptr, &mb);
		auto pixmap = icon.pixmap(iconSize);
		mb.setIconPixmap(pixmap);
		mb.setInformativeText(tr("You can always check for updates manually from the Help menu."));
		const auto result = mb.exec();
		dpApp().settings().setVersionCheckEnabled(result == QMessageBox::Yes);
		dpApp().settings().setVersionCheckFirstRun(true);
	}

	if(NewVersionCheck::needCheck(dpApp().settings())) {
		// The dialog will autodelete if there is nothing to show
		VersionCheckDialog *dlg = new VersionCheckDialog;
		dlg->queryNewVersions();
	}
}

void VersionCheckDialog::queryNewVersions()
{
	m_newversion = new NewVersionCheck(this);
	connect(m_newversion, &NewVersionCheck::versionChecked, this, &VersionCheckDialog::versionChecked);
	m_newversion->queryVersions(dpApp().settings());
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
		QString h1 = tr("Error checking version!").toHtmlEscaped();
		QString p = errorMessage.toHtmlEscaped();
		m_ui->textBrowser->insertHtml(QStringLiteral("<h1>%1</h1><p>%2</p>").arg(h1, p));
		m_ui->views->setCurrentIndex(1);
		showButtons(QDialogButtonBox::Ok);

	} else {
		setNewVersions(m_newversion->getNewer());
	}

	show();
}

void VersionCheckDialog::setNewVersions(const QVector<NewVersionCheck::Version> &versions)
{
	if(versions.isEmpty()) {
		QString h1 = tr("You're up to date!").toHtmlEscaped();
		QString p = tr("No new versions found.").toHtmlEscaped();
		m_ui->textBrowser->setHtml(QStringLiteral("<h1>%1</h1><p>%2</p>").arg(h1, p));
		showButtons(QDialogButtonBox::Ok);

	} else {
		const NewVersionCheck::Version *latestStable = nullptr;
		for(const auto &version : versions) {
			if(!latestStable && version.stable) {
				latestStable = &version;
			}
		}

		m_latest = latestStable ? *latestStable : versions.first();
		QString h1 = tr("A new version of Drawpile is available!").toHtmlEscaped();
		QString h2 = tr("Version %2").arg(m_latest.version).toHtmlEscaped();
		QString content =
			QStringLiteral("<h1>%1</h1><h2><a href=\"%2\">%3</a></h2>%4")
				.arg(h1, m_latest.version, h2, m_latest.description);
		m_ui->textBrowser->setHtml(content);
		showButtons(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
	}
	m_ui->views->setCurrentIndex(1);
}

void VersionCheckDialog::downloadNewVersion()
{
	Q_ASSERT(!m_latest.version.isEmpty());
	QString url{QStringLiteral("https://drawpile.net/download/%1")
					.arg(m_latest.stable ? "" : "#Beta")};
	QDesktopServices::openUrl(url);
	QString href = url.toHtmlEscaped();
	//: %2 is used twice, this is intentional! Don't use %3.
	m_ui->textBrowser->setHtml(
		tr("<p>The download page for Drawpile %1 should have opened in your "
		   "browser. If not, visit <a href=\"%2\">%2</a> manually.</p>"
		   "<p>Restart Drawpile after installing the new version.</p>")
			.arg(m_latest.version, href));
	showButtons(QDialogButtonBox::Ok);
}

void VersionCheckDialog::showButtons(const QDialogButtonBox::StandardButtons &buttons)
{
	QAbstractButton *okButton = m_ui->buttonBox->button(QDialogButtonBox::Ok);
	okButton->setVisible(buttons.testFlag(QDialogButtonBox::Ok));
	QAbstractButton *cancelButton = m_ui->buttonBox->button(QDialogButtonBox::Cancel);
	cancelButton->setVisible(buttons.testFlag(QDialogButtonBox::Cancel));
	m_downloadButton->setVisible(buttons.testFlag(QDialogButtonBox::Save));

	if(m_downloadButton->isVisible()) {
		m_downloadButton->setFocus();
	} else if(okButton->isVisible()) {
		okButton->setFocus();
	}
}

}
