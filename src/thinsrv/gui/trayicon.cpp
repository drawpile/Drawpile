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

#include "thinsrv/gui/trayicon.h"
#include "thinsrv/gui/mainwindow.h"
#include "thinsrv/gui/localserver.h"
#include "thinsrv/gui/remoteserver.h"

#include <QMenu>
#include <QApplication>
#include <QPainter>
#include <QFont>
#include <QInputDialog>

namespace server {
namespace gui {

static TrayIcon *INSTANCE;

TrayIcon *TrayIcon::instance()
{
	if(!INSTANCE)
		INSTANCE = new TrayIcon;
	return INSTANCE;
}

bool TrayIcon::isTrayIconVisible()
{
	return QSystemTrayIcon::isSystemTrayAvailable() &&
			INSTANCE &&
			INSTANCE->isVisible();
}

void TrayIcon::showTrayIcon()
{
	if(!isTrayIconVisible()) {
		instance()->updateIcon();
		instance()->show();
	}
}

void TrayIcon::hideTrayIcon()
{
	if(isTrayIconVisible())
		instance()->hide();
}

TrayIcon::TrayIcon()
	: QSystemTrayIcon(),
	  m_number(0),
	  m_serverOn(false),
	  m_needIconRefresh(true)
{
	QMenu *menu = new QMenu;
	QAction *showAction = menu->addAction(tr("Show"));
	QAction *remoteAction = menu->addAction(tr("Remote..."));
	QAction *quitAction = menu->addAction(tr("Quit"));

	// In Qt 5.6 and up we can just do addAction(title, &functor)
	connect(showAction, &QAction::triggered, &MainWindow::showDefaultInstance);
	connect(remoteAction, &QAction::triggered, this, &TrayIcon::openRemote);
	connect(quitAction, &QAction::triggered, this, &TrayIcon::quitServer);

	setContextMenu(menu);

#ifndef Q_OS_MACOS
	connect(this, &TrayIcon::activated, this, &TrayIcon::onActivated);
#endif

	m_icon = QImage(":/icons/icon.svg", "SVG");
}

void TrayIcon::setNumber(int num)
{
	if(m_number != num) {
		m_number = num;
		m_needIconRefresh = true;
		if(isTrayIconVisible())
			updateIcon();
	}
}

void TrayIcon::setServerOn(bool on)
{
	if(m_serverOn != on) {
		m_serverOn = on;
		m_needIconRefresh = true;
		if(isTrayIconVisible())
			updateIcon();
	}
}

void TrayIcon::updateIcon()
{
	if(!m_needIconRefresh)
		return;

	QPixmap icon(m_icon.size());
	icon.fill(Qt::transparent);
	{
		QPainter painter(&icon);
		if(!m_serverOn)
			painter.setOpacity(0.5);
		painter.drawImage(0, 0, m_icon);
		painter.setOpacity(1);

		if(m_number>0) {
			QFont font;
			font.setPixelSize(icon.height() * 2 / 3);
			painter.setFont(font);
			painter.setBrush(Qt::red);

			QString num = QString::number(m_number);
			QFontMetrics fm(font);
			QRect rect = fm.boundingRect(num);

			rect.moveTo(icon.width()/2 - rect.width()/2, icon.height()/2-rect.height()/2);

			painter.setPen(Qt::NoPen);
			painter.drawRoundedRect(rect.adjusted(-3, -3, 3, -3), rect.height()/2, rect.height()/2);

			painter.setPen(Qt::white);
			painter.drawText(rect, Qt::AlignCenter, num);
		}
	}

	setIcon(QIcon(icon));

	m_needIconRefresh = false;
}

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
	switch(reason) {
	case QSystemTrayIcon::DoubleClick:
	case QSystemTrayIcon::Trigger:
		MainWindow::showDefaultInstance();
		break;
	default: break;
	}
}

void TrayIcon::openRemote()
{
	QString url = QInputDialog::getText(nullptr, tr("Remote Admin"), tr("Remote server admin URL"));
	if(!url.isEmpty()) {
		QUrl remoteUrl(url);
		if(remoteUrl.isValid()) {
			RemoteServer *server = new RemoteServer(remoteUrl);
			MainWindow *win = new MainWindow(server);
			server->setParent(win);
			win->show();
		}
	}
}

void TrayIcon::quitServer()
{
	LocalServer *srv = qobject_cast<LocalServer*>(MainWindow::defaultInstanceServer());
	if(srv && srv->isRunning()) {
		srv->confirmQuit();
	} else {
		qApp->exit();
	}
}

}
}
