// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_STARTDIALOG_HOST_H
#define DESKTOP_DIALOGS_STARTDIALOG_HOST_H

#include "desktop/dialogs/startdialog/page.h"
#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
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

public slots:
	void setHostEnabled(bool enabled);
	void updateHostEnabled();

signals:
	void showButtons();
	void enableHost(bool enabled);
	void host(
		const QString &title, const QString &password, const QString &alias,
		bool nsfm, const QString &announcementUrl,
		const QString &remoteAddress);

private slots:
	void generatePassword();
	void updateNsfmBasedOnTitle();
	void updateListServers();
	void updateRemoteHosts();
	void updateAdvancedSectionVisible(bool visible);

private:
	static constexpr int USE_LOCAL = 0;
	static constexpr int USE_REMOTE = 1;

	bool canHost() const;

	QString getRemoteAddress() const;

	QLineEdit *m_titleEdit;
	QLineEdit *m_passwordEdit;
	QCheckBox *m_nsfmBox;
	QCheckBox *m_announceBox;
	QButtonGroup *m_useGroup;
	QComboBox *m_remoteHostCombo;
	QCheckBox *m_advancedBox;
	QFormLayout *m_advancedSection;
	QLabel *m_idAliasLabel;
	QLineEdit *m_idAliasEdit;
	QComboBox *m_listServerCombo;
	bool m_allowNsfm = true;
	sessionlisting::ListServerModel *m_listServerModel;
};

}
}

#endif
