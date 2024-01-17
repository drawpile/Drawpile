// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_INVITEDIALOG_H
#define DESKTOP_DIALOGS_INVITEDIALOG_H

#include <QDialog>

namespace widgets {
class NetStatus;
}

namespace dialogs {

class InviteDialog : public QDialog {
	Q_OBJECT
public:
	enum class LinkType : int { Web, Direct };
	Q_ENUM(LinkType)

	InviteDialog(
		widgets::NetStatus *netStatus, bool compatibilityMode, bool allowWeb,
		bool nsfm, QWidget *parent);

	~InviteDialog() override;

public slots:
	void setSessionCompatibilityMode(bool compatibilityMode);
	void setSessionAllowWeb(bool allowWeb);
	void setSessionNsfm(bool nsfm);

private slots:
	void copyInviteLink();
	void discoverAddress();
	void updatePage();
	void updateInviteLink();

private:
	static constexpr int URL_PAGE_INDEX = 0;
	static constexpr int IP_PAGE_INDEX = 1;

	struct Private;
	Private *d;
};

}

#endif
