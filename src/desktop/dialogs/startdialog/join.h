// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_STARTDIALOG_JOIN_H
#define DESKTOP_DIALOGS_STARTDIALOG_JOIN_H

#include "desktop/dialogs/startdialog/page.h"
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

class QGraphicsOpacityEffect;
class QLabel;
class QLineEdit;
class QUrl;

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

protected:
	bool eventFilter(QObject *object, QEvent *event) final override;

private slots:
	void addressChanged(const QString &address);

private:
	static constexpr char SCHEME[] = "drawpile://";

	void resetAddressPlaceholderText();
	void updateRecentHosts(const QStringList &recentHosts);
	void updateJoinButton();

	static QGraphicsOpacityEffect *makeOpacityEffect(double opacity);

	static bool looksLikeRoomcode(const QString &address);
	static QString fixUpInviteAddress(const QString &address);

	QStringList getRoomcodeServerUrls() const;
	void resolveRoomcode(const QString &roomcode, const QStringList &servers);
	void finishResolvingRoomcode(const QString &address);

	QUrl getUrl() const;

	QLineEdit *m_addressEdit;
	QLabel *m_addressMessageLabel;
	QVBoxLayout *m_recentHostsLayout;
	QVector<QLabel *> m_recentHostsLabels;
};

}
}

#endif
