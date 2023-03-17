/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

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
