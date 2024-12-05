// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_STARTDIALOG_HOST_ROLES_H
#define DESKTOP_DIALOGS_STARTDIALOG_HOST_ROLES_H
#include "desktop/utils/qtguicompat.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QTableView;

namespace net {
class AuthListModel;
}

namespace dialogs {
namespace startdialog {
namespace host {

class Roles : public QWidget {
	Q_OBJECT
public:
	explicit Roles(QWidget *parent = nullptr);

	void reset(bool replaceAuth);
	void load(const QJsonObject &json, bool replaceAuth);
	QJsonObject save() const;

	void host(QString &outOperatorPassword, QJsonArray &outAuth);

private:
	struct AuthListEntry {
		QString authId;
		QString username;
		bool op;
		bool trusted;
	};

	void opBoxChanged(compat::CheckBoxState state);
	void trustedBoxChanged(compat::CheckBoxState state);
	void updateAuthListCheckboxes();
	QModelIndex getSelectedAuthListEntry();
	void updateAuthListVisibility();
	void saveAuthList();
	QJsonArray authListToJson() const;

	QLineEdit *m_operatorPasswordEdit;
	QLabel *m_noAuthListLabel;
	QWidget *m_authListWidget;
	net::AuthListModel *m_authListModel;
	QTableView *m_authListTable;
	QCheckBox *m_opBox;
	QCheckBox *m_trustedBox;
};

}
}
}

#endif
