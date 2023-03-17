/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017-2019 Calle Laakkonen

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

#ifndef SESSIONSETTINGSDIALOG_H
#define SESSIONSETTINGSDIALOG_H

#include "libclient/canvas/acl.h"

#include <QDialog>
#include <QJsonObject>

class QStringListModel;
class QTimer;
class QLabel;
class QComboBox;

class Ui_SessionSettingsDialog;
class Document;

namespace canvas { class CanvasModel; }

namespace dialogs {

class SessionSettingsDialog final : public QDialog
{
	Q_OBJECT
public:
	SessionSettingsDialog(Document *doc, QWidget *parent=nullptr);
	~SessionSettingsDialog() override;

	//! Is persistence available at all on this server?
	void setPersistenceEnabled(bool);

	//! Is autoreset supported?
	void setAutoResetEnabled(bool);

	//! Is the local user authenticated/not a guest?
	void setAuthenticated(bool);

signals:
	void requestAnnouncement(const QString &apiUrl);
	void requestUnlisting(const QString &apiUrl);

private slots:
	void onCanvasChanged(canvas::CanvasModel*);
	void onOperatorModeChanged(bool op);
	void onFeatureTiersChanged(const DP_FeatureTiers &features);

	void permissionPresetSaving(const QString &);
	void permissionPresetSelected(const QString &);

	void titleChanged(const QString &newTitle);

	void maxUsersChanged();
	void denyJoinsChanged(bool);
	void authOnlyChanged(bool);

	void permissionChanged();

	void autoresetThresholdChanged();
	void keepChatChanged(bool);
	void persistenceChanged(bool);
	void nsfmChanged(bool);
	void deputiesChanged(int);

	void changePassword();
	void changeOpword();

	void changeSesionConf(const QString &key, const QJsonValue &value, bool now=false);
	void sendSessionConf();

	void updatePasswordLabel(QLabel *label);
	void updateNsfmCheckbox(bool);

protected:
	void showEvent(QShowEvent *event) override;

private:
	void initPermissionComboBoxes();
	void reloadSettings();
	QComboBox *featureBox(DP_Feature f);

	Ui_SessionSettingsDialog *m_ui;
	Document *m_doc;
	QTimer *m_saveTimer;

	QJsonObject m_sessionconf;
	bool m_featureTiersChanged = false;

	bool m_op = false;
	bool m_isAuth = false;
	bool m_canPersist = false;
	bool m_canAutoreset = false;
};

}

#endif
