/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2018 Calle Laakkonen

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
#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

class Ui_LoginDialog;

class QUrl;
class QSslCertificate;

namespace net {
	class LoginHandler;
	class LoginSessionModel;
}

namespace dialogs {

/**
 * @brief The dialog that is shown during the login process
 *
 * This dialog handles all the user interaction needed while logging in.
 */
class LoginDialog final : public QDialog
{
	Q_OBJECT
public:
	explicit LoginDialog(net::LoginHandler *login, QWidget *parent=nullptr);
	~LoginDialog() override;

public slots:
	void catchupProgress(int value);
	void onLoginDone(bool join);

private slots:
	void onOkClicked();
	void onReportClicked();

	void updateOkButtonEnabled();

	void showOldCert();
	void showNewCert();

	void onUsernameNeeded(bool canSelectAvatar);
	void onLoginNeeded(const QString &forUsername, const QString &prompt);
	void onExtAuthNeeded(const QString &forUsername, const QUrl &url);
	void onExtAuthComplete(bool success);
	void onSessionChoiceNeeded(net::LoginSessionModel *sessions);
	void onSessionPasswordNeeded();
	void onLoginOk();
	void onBadLoginPassword();
	void onCertificateCheckNeeded(const QSslCertificate &newCert, const QSslCertificate &oldCert);
	void onServerTitleChanged(const QString &title);

private:
	struct Private;
	Private *d;
};

}

#endif // LOGINDIALOG_H
