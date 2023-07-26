// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_STARTDIALOG_JOIN_H
#define DESKTOP_DIALOGS_STARTDIALOG_JOIN_H

#include "desktop/dialogs/startdialog/page.h"
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

class QLabel;
class QLineEdit;
class QUrl;

namespace widgets {
class RecentScroll;
}

namespace dialogs {
namespace startdialog {

class Join final : public Page {
	Q_OBJECT
public:
	Join(QWidget *parent = nullptr);
	void activate() final override;
	void accept() final override;

public slots:
	void setAddress(const QString &address);

signals:
	void showButtons();
	void enableJoin(bool enabled);
	void join(const QUrl &url);

private slots:
	void acceptAddress(const QString &address);
	void addressChanged(const QString &address);

private:
	static constexpr char SCHEME[] = "drawpile://";

	void resetAddressPlaceholderText();
	void updateJoinButton();

	static bool looksLikeRoomcode(const QString &address);
	static QString fixUpInviteAddress(const QString &address);

	QStringList getRoomcodeServerUrls() const;
	void resolveRoomcode(const QString &roomcode, const QStringList &servers);
	void finishResolvingRoomcode(const QString &address);

	QUrl getUrl() const;

	QLineEdit *m_addressEdit;
	QLabel *m_addressMessageLabel;
	widgets::RecentScroll *m_recentScroll;
};

}
}

#endif
