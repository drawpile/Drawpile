// SPDX-License-Identifier: GPL-3.0-or-later

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
	void onYesClicked();
	void onNoClicked();

	void updateOkButtonEnabled();

	void updateAvatar(int row);
	void pickNewAvatar(const QModelIndex &parent, int first, int last);

	void showOldCert();
	void showNewCert();

	void onUsernameNeeded(bool canSelectAvatar);
	void onLoginNeeded(const QString &forUsername, const QString &prompt);
	void onExtAuthNeeded(const QString &forUsername, const QUrl &url);
	void onExtAuthComplete(bool success);
	void onSessionChoiceNeeded(net::LoginSessionModel *sessions);
	void onSessionConfirmationNeeded(const QString &title, bool nsfm, bool autoJoin);
	void onSessionPasswordNeeded();
	void onLoginOk();
	void onBadLoginPassword();
	void onCertificateCheckNeeded(const QSslCertificate &newCert, const QSslCertificate &oldCert);
	void onServerTitleChanged(const QString &title);

private:
	void adjustSize(int width, int height, bool allowShrink);
	void selectCurrentAvatar();

	struct Private;
	Private *d;
};

}

#endif // LOGINDIALOG_H
