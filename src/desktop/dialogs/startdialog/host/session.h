// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_STARTDIALOG_HOST_SESSION_H
#define DESKTOP_DIALOGS_STARTDIALOG_HOST_SESSION_H
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTextBrowser;

namespace dialogs {
namespace startdialog {
namespace host {

class Session : public QWidget {
	Q_OBJECT
public:
	enum class Type { Passworded, Public };

	explicit Session(bool connected, QWidget *parent = nullptr);

private:
	void updateType(int type);

	static void generatePassword();

	QComboBox *m_typeCombo;
	QLabel *m_passwordLabel;
	QLineEdit *m_passwordEdit;
	QPushButton *m_passwordButton;
	QCheckBox *m_nsfmBox;
	QCheckBox *m_ephemeralBox;
	QCheckBox *m_keepChatBox;
	QComboBox *m_serverCombo = nullptr;
	QLineEdit *m_hostEdit = nullptr;
	QTextBrowser *m_serverInfo = nullptr;
	bool m_connected;
};

}
}
}

#endif
