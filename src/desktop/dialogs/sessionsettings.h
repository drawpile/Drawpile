/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017-2018 Calle Laakkonen

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

#include "canvas/features.h"

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

class SessionSettingsDialog : public QDialog
{
	Q_OBJECT
public:
	SessionSettingsDialog(Document *doc, QWidget *parent=nullptr);
	~SessionSettingsDialog();

	//! Is persistence available at all on this server?
	void setPersistenceEnabled(bool);

	//! Is the local user authenticated/not a guest?
	void setAuthenticated(bool);

signals:
	void requestAnnouncement(const QString &apiUrl);
	void requestUnlisting(const QString &apiUrl);

private slots:
	void onCanvasChanged(canvas::CanvasModel*);
	void onOperatorModeChanged(bool op);
	void onFeatureTierChanged(canvas::Feature feature, canvas::Tier tier);

	void titleChanged(const QString &newTitle);

	void maxUsersChanged();
	void denyJoinsChanged(bool);
	void authOnlyChanged(bool);

	void permissionChanged();

	void autoresetThresholdChanged();
	void keepChatChanged(bool);
	void persistenceChanged(bool);
	void nsfmChanged(bool);

	void changePassword();
	void changeOpword();

	void changeSesionConf(const QString &key, const QJsonValue &value, bool now=false);
	void sendSessionConf();

	void updatePasswordLabel(QLabel *label);

private:
	void initPermissionComboBoxes();
	QComboBox *featureBox(canvas::Feature f);

	Ui_SessionSettingsDialog *m_ui;
	Document *m_doc;
	QTimer *m_saveTimer;

	QJsonObject m_sessionconf;
	bool m_featureTiersChanged;

	bool m_op;
	bool m_isAuth;
	bool m_canPersist;
};

}

#endif
