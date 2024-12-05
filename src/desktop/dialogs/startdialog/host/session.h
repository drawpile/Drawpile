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
	enum class Type { Hidden, Visible, Public };

	explicit Session(QWidget *parent = nullptr);

private:
    static QLabel *makeSpacerLabel(const QString &text);
    void updateType(int type);

	QComboBox *m_typeCombo;
    QLabel *m_titleLabel;
	QLineEdit *m_titleEdit;
    QLabel *m_passwordLabel;
	QLineEdit *m_passwordEdit;
	QPushButton *m_passwordButton;
	QCheckBox *m_nsfmBox;
	QCheckBox *m_persistentBox;
	QCheckBox *m_keepChatBox;
	QStackedWidget *m_stack;
	QComboBox *m_serverCombo;
	QLineEdit *m_hostEdit;
	QTextBrowser *m_serverInfo;
};

}
}
}

#endif
