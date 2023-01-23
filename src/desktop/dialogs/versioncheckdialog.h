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

#ifndef VERSIONCHECKDIALOG_H
#define VERSIONCHECKDIALOG_H

#include "libclient/utils/newversion.h"

#include <QDialog>

class Ui_VersionCheckDialog;
class QPushButton;

namespace dialogs {

class VersionCheckDialog : public QDialog
{
	Q_OBJECT
public:
	explicit VersionCheckDialog(QWidget *parent=nullptr);
	~VersionCheckDialog();

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
	void rememberSettings();
	void downloadNewVersion();

private:
	Ui_VersionCheckDialog *m_ui;
	NewVersionCheck *m_newversion;
	QPushButton *m_downloadButton;

	QUrl m_downloadUrl;
	QByteArray m_downloadSha256;
	int m_downloadSize;
};

}

#endif // VERSIONCHECKDIALOG_H
