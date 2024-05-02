// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H
#include "libclient/net/login.h"
#include <QDialog>

class Ui_LoginDialog;

class QUrl;
class QSslCertificate;

namespace net {
class LoginSessionModel;
}

namespace dialogs {

/**
 * @brief The dialog that is shown during the login process
 *
 * This dialog handles all the user interaction needed while logging in.
 */
class LoginDialog final : public QDialog {
	Q_OBJECT
public:
	explicit LoginDialog(net::LoginHandler *login, QWidget *parent = nullptr);
	~LoginDialog() override;

	static void showJoinDenyMessageBox(
		QWidget *parent, const QStringList &reasons, const QIcon &icon);

public slots:
	void catchupProgress(int value);
	void onLoginDone(bool join);

private slots:
	void onOkClicked();
	void onReportClicked();
	void onYesClicked();
	void onNoClicked();

	void updateOkButtonEnabled();

	void onRecentLogInClicked();
	void onRecentEditClicked();
	void onRecentForgetClicked();
	void onRecentBreakClicked();

	void onLoginMethodExtAuthClicked();
	void onLoginMethodAuthClicked();
	void onLoginMethodGuestClicked();

	void updateAvatar(int row);
	void pickNewAvatar(const QModelIndex &parent, int first, int last);

	void showOldCert();
	void showNewCert();

	void onRuleAcceptanceNeeded(const QString &ruleText);
	void onLoginMethodChoiceNeeded(
		const QVector<net::LoginHandler::LoginMethod> &methods, const QUrl &url,
		const QUrl &extAuthUrl, const QString &loginInfo);
	void onLoginMethodMismatch(
		net::LoginHandler::LoginMethod intent,
		net::LoginHandler::LoginMethod method, bool extAuthFallback);
	void onUsernameNeeded(bool canSelectAvatar);
	void onLoginNeeded(
		const QString &forUsername, const QString &prompt, const QString &host,
		net::LoginHandler::LoginMethod intent);
	void onExtAuthNeeded(
		const QString &forUsername, const QUrl &url, const QString &host,
		net::LoginHandler::LoginMethod intent);
	void onExtAuthComplete(
		bool success, net::LoginHandler::LoginMethod intent,
		const QString &host, const QString &username);
	void onSessionChoiceNeeded(net::LoginSessionModel *sessions);
	void
	onSessionConfirmationNeeded(const QString &title, bool nsfm, bool autoJoin);
	void onSessionPasswordNeeded();
	void onBadSessionPassword();
	void onLoginOk(
		net::LoginHandler::LoginMethod intent, const QString &host,
		const QString &username);
	void onBadLoginPassword(
		net::LoginHandler::LoginMethod intent, const QString &host,
		const QString &username);
	void onCertificateCheckNeeded(
		const QSslCertificate &newCert, const QSslCertificate &oldCert);
	void onServerTitleChanged(const QString &title);

	void onPasswordReadFinished(int jobId, const QString &password);

private:
	static constexpr char LAST_GUEST_NAME_KEY[] = "login/lastguestname";
	static constexpr char LAST_GUEST_AVATAR_KEY[] = "login/lastguestavatar";

	void adjustSize(int width, int height, bool allowShrink);
	void selectCurrentAvatar();
	void selectRecentAccount(bool logIn);
	void setAuthLoginExplanation();
	void setExtAuthLoginExplanation();
	void delayUpdate();
	static QString formatLoginInfo(const QString &loginInfo);
	static QString formatExtAuthPrompt(const QUrl &url);

	struct Private;
	Private *d;
};

}

#endif // LOGINDIALOG_H
