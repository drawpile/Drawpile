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

#include <QDialog>
#include <QJsonObject>

class QStringListModel;
class QTimer;
class QLabel;
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

	void titleChanged(const QString &newTitle);

	void maxUsersChanged();
	void denyJoinsChanged(bool);
	void authOnlyChanged(bool);
	void lockNewUsersChanged(bool);

	void lockImagesChanged(bool);
	void lockAnnotationsChanged(bool);
	void lockLayerCtrlChanged(bool);
	void ownLayersChanged(bool);

	void keepChatChanged(bool);
	void persistenceChanged(bool);
	void nsfmChanged(bool);

	void changePassword();
	void changeOpword();

	void changeSesionConf(const QString &key, const QJsonValue &value, bool now=false);
	void changeSessionAcl(uint16_t flag, bool set);
	void sendSessionConf();

	void updatePasswordLabel(QLabel *label);

private:
	Ui_SessionSettingsDialog *m_ui;
	Document *m_doc;
	QTimer *m_saveTimer;

	QJsonObject m_sessionconf;
	uint16_t m_aclFlags, m_aclMask;
	bool m_op;
	bool m_isAuth;
	bool m_canPersist;
};

}

#endif
