// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_STARTDIALOG_HOST_SESSION_H
#define DESKTOP_DIALOGS_STARTDIALOG_HOST_SESSION_H
#include <QIcon>
#include <QJsonObject>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTextBrowser;

namespace desktop {
namespace settings {
class Settings;
}
}

namespace utils {
class FormNote;
}

namespace widgets {
class ImageResourceTextBrowser;
}

namespace dialogs {
namespace startdialog {
namespace host {

class Session : public QWidget {
	Q_OBJECT
public:
	enum class Type { Passworded, Public };

	explicit Session(QWidget *parent = nullptr);

	void reset();
	void load(const QJsonObject &json);
	QJsonObject save() const;

	bool isPersonal() const;

	void host(
		QStringList &outErrors, QString &outPassword, bool &outNsfm,
		bool &outKeepChat, QString &outAddress, bool &outRememberAddress);

	bool isNsfmAllowed() const;
	void setNsfmBasedOnListing();

signals:
	void personalChanged(bool personal);
	void requestTitleEdit();

private:
	static constexpr int SERVER_PUB = 0;
	static constexpr int SERVER_ADDRESS = 1;
	static constexpr int SERVER_BUILTIN = 2;

	void updateType(int type);
	void updateServer(int server);
	void showServerPub();
	void showServerAddress();
#ifdef DP_HAVE_BUILTIN_SERVER
	void showServerBuiltin();
#endif
	void updateAddressCombo();

	void followServerInfoLink(const QUrl &url);

	static void generatePassword();
	static void generatePasswordWith(desktop::settings::Settings &settings);
	static void fixUpLastHostServer(desktop::settings::Settings &settings);
	static bool looksLikePub(const QString &address);

	QIcon m_drawpileIcon;
	QComboBox *m_typeCombo;
	utils::FormNote *m_titleNote;
	QLabel *m_passwordLabel;
	QLineEdit *m_passwordEdit;
	QPushButton *m_passwordButton;
	QCheckBox *m_nsfmBox;
	QCheckBox *m_keepChatBox;
	QComboBox *m_serverCombo = nullptr;
	QComboBox *m_addressCombo = nullptr;
	widgets::ImageResourceTextBrowser *m_serverInfo = nullptr;
	int m_server = SERVER_PUB;
};

}
}
}

#endif
