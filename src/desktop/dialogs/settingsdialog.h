/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2018 Calle Laakkonen

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
#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QSslCertificate>
#include <QMap>

class Ui_SettingsDialog;
class QListWidgetItem;
class CustomShortcutModel;
class AvatarListModel;

namespace sessionlisting { class ListServerModel; }

namespace dialogs {

class SettingsDialog : public QDialog
{
Q_OBJECT
public:
	SettingsDialog(QWidget *parent=nullptr);
	~SettingsDialog();

private slots:
	void resetSettings();
	void rememberSettings();
	void saveCertTrustChanges();

	void viewCertificate(QListWidgetItem *item);
	void markTrustedCertificates();
	void removeCertificates();
	void certificateSelectionChanged();
	void importTrustedCertificate();

	void addListingServer();
	void removeListingServer();
	void moveListingServerUp();
	void moveListingServerDown();

	void lockParentalControls();

	void addAvatar();
	void removeSelectedAvatar();

private:
	void restoreSettings();
	void setParentalControlsLocked(bool lock);
	void rememberPcLevel();

	Ui_SettingsDialog *m_ui;

	QStringList m_removeCerts;
	QStringList m_trustCerts;
	QList<QSslCertificate> m_importCerts;
	CustomShortcutModel *m_customShortcuts;
	sessionlisting::ListServerModel *m_listservers;
	AvatarListModel *m_avatars;
};

}

#endif

