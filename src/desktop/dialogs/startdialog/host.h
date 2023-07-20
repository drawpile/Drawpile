// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_STARTDIALOG_HOST_H
#define DESKTOP_DIALOGS_STARTDIALOG_HOST_H

#include "desktop/dialogs/startdialog/page.h"
#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLineEdit;

namespace sessionlisting {
class ListServerModel;
}

namespace dialogs {
namespace startdialog {

class Host final : public Page {
	Q_OBJECT
public:
	Host(QWidget *parent = nullptr);
	void activate() final override;
	void accept() final override;

signals:
	void showButtons();
	void enableHost(bool enabled);
	void host(
		const QString &title, const QString &password, const QString &alias,
		bool nsfm, const QString &announcementUrl,
		const QString &remoteAddress);

private slots:
	void updateHostEnabled();
	void updateNsfmBasedOnTitle();
	void updateListServers();

private:
	static constexpr int USE_LOCAL = 0;
	static constexpr int USE_REMOTE = 1;

	bool canHost() const;

	QString getRemoteAddress() const;

	QLineEdit *m_titleEdit;
	QLineEdit *m_passwordEdit;
	QLineEdit *m_idAliasEdit;
	QCheckBox *m_nsfmBox;
	QCheckBox *m_announceBox;
	QComboBox *m_listServerCombo;
	QButtonGroup *m_useGroup;
	QComboBox *m_remoteHostCombo;
	bool m_allowNsfm = true;
	sessionlisting::ListServerModel *m_listServerModel;
};

}
}

#endif
