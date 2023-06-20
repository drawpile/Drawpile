// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VERSIONCHECKDIALOG_H
#define VERSIONCHECKDIALOG_H

#include "libclient/utils/newversion.h"

#include <QDialog>
#include <QDialogButtonBox>

class Ui_VersionCheckDialog;
class QPushButton;

namespace dialogs {

class VersionCheckDialog final : public QDialog
{
	Q_OBJECT
public:
	explicit VersionCheckDialog(QWidget *parent=nullptr);
	~VersionCheckDialog() override;

	/**
	 * @brief Perform version check (if needed) and show dialog if new version is available
	 */
	static void doVersionCheckIfNeeded();

	/**
	 * @brief Set the list of new versions available
	 *
	 * If the list is empty, a "no new versions" message is shown instead.
	 *
	 * @param versions
	 */
	void setNewVersions(const QVector<NewVersionCheck::Version> &versions);

	//! Check for new versions and update the dialog's content based on the results
	void queryNewVersions();

private slots:
	void versionChecked(bool isNew, const QString &errorMessage);
	void downloadNewVersion();

private:
	void showButtons(const QDialogButtonBox::StandardButtons &buttons);

	Ui_VersionCheckDialog *m_ui;
	NewVersionCheck *m_newversion;
	QPushButton *m_downloadButton;
	NewVersionCheck::Version m_latest;
};

}

#endif // VERSIONCHECKDIALOG_H
