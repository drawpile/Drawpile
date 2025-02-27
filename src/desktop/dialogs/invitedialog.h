// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_INVITEDIALOG_H
#define DESKTOP_DIALOGS_INVITEDIALOG_H
#include <QDialog>
#include <QSet>

class QButtonGroup;
class QDialogButtonBox;
class QSpinBox;

namespace net {
class InviteListModel;
}

namespace widgets {
class NetStatus;
}

namespace dialogs {

class InviteDialog : public QDialog {
	Q_OBJECT
public:
	InviteDialog(
		widgets::NetStatus *netStatus, net::InviteListModel *inviteListModel,
		bool compatibilityMode, bool allowWeb, bool nsfm, bool op,
		bool moderator, bool supportsCodes, bool codesEnabled, QWidget *parent);

	~InviteDialog() override;

	void setSessionCompatibilityMode(bool compatibilityMode);
	void setSessionAllowWeb(bool allowWeb);
	void setSessionNsfm(bool nsfm);
	void setOp(bool op);
	void setSessionCodesEnabled(bool codesEnabled);
	void setServerSupportsInviteCodes(bool supportsCodes);
	void selectInviteCode(const QString &secret);

signals:
	void createInviteCode(int maxUses, bool op, bool trust);
	void removeInviteCode(const QString &secret);

private:
	QString
	buildWebInviteLink(bool includePassword, const QString &secret) const;
	static QString buildPath(QString path, const QString &secret);
	void copyInviteLink();
	int copyInviteCodeLinks();
	void copyInviteCodeLinksWithMessage();
	void discoverAddress();
	void updatePage();
	void updateInviteLink();
	void updateCodes();
	void showCreateCodeDialog();
	void promptRemoveSelectedCodes();
	void removeSelectedCodes();
	QSet<QString> gatherSelectedSecrets();
	void showCodeExplanation();
	void emitCreateInviteCode(int maxUses, bool op, bool trust);
	void showInviteCodeContextMenu(const QPoint &pos);

	static constexpr int URL_PAGE_INDEX = 0;
	static constexpr int IP_PAGE_INDEX = 1;

	struct Private;
	Private *d;
};

class CreateInviteCodeDialog : public QDialog {
	Q_OBJECT
public:
	explicit CreateInviteCodeDialog(QWidget *parent = nullptr);

signals:
	void createInviteCode(int maxUses, bool op, bool trust);

private:
	static constexpr int ROLE_NONE = 0;
	static constexpr int ROLE_TRUST = 1;
	static constexpr int ROLE_OP = 2;

	void emitCreateInviteCode();

	QSpinBox *m_maxUsesSpinner;
	QButtonGroup *m_roleGroup;
	QDialogButtonBox *m_buttons;
};

}

#endif
