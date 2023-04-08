// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TRAYICON_H
#define TRAYICON_H

#include <QSystemTrayIcon>

namespace server {
namespace gui {

class TrayIcon final : public QSystemTrayIcon
{
	Q_OBJECT
public:
	static TrayIcon *instance();
	static bool isTrayIconVisible();
	static void showTrayIcon();
	static void hideTrayIcon();

public slots:
	void setNumber(int userCount);
	void setServerOn(bool on);

private slots:
	void onActivated(QSystemTrayIcon::ActivationReason reason);
	void quitServer();
	void openRemote();

private:
	TrayIcon();
	void updateIcon();

	int m_number;
	bool m_serverOn;
	bool m_needIconRefresh;
	QImage m_icon;
};

}
}

#endif
