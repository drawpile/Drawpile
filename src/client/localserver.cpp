/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2008 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#include <QDebug>
#include <QNetworkInterface>
#include <QSettings>
#include <QDir>

#include "main.h"
#include "localserver.h"
#include "../shared/net/constants.h"
#include "../shared/server/server.h"

LocalServer *LocalServer::instance_;

LocalServer::LocalServer() : QThread()
{
}

bool isGlobal(const QHostAddress& address) {
	// This could be a bit more comprehensive
	if(address.protocol() == QAbstractSocket::IPv6Protocol) {
		if(address.scopeId()=="Global")
			return true;
		return false;
	} else {
		quint32 a = address.toIPv4Address();
		return !((a >> 24) == 10 ||
			(a >> 16) == 0xC0A8);
	}
}

bool addressSort(const QHostAddress& a1, const QHostAddress& a2)
{
	if(a1 == QHostAddress::LocalHost || a1 == QHostAddress::LocalHostIPv6)
		return false;
	return !isGlobal(a1);
}

/**
 * Attempt to discover the address most likely reachable from the
 * outside.
 * @return server hostname
 */
QString LocalServer::address()
{
	QList<QNetworkInterface> list = QNetworkInterface::allInterfaces();
	QList<QHostAddress> alist;
        
	// Gather a list of acceptable addresses
	foreach (QNetworkInterface iface, list) {
		if(!(iface.flags() & QNetworkInterface::IsUp) ||
				!(iface.flags() & QNetworkInterface::IsRunning))
			continue;

		QList<QNetworkAddressEntry> addresses = iface.addressEntries();

		foreach (QNetworkAddressEntry entry, addresses) {
			alist.append(entry.ip());
        }
	}

	if (alist.count() > 0) {
		qSort(alist.begin(), alist.end(), addressSort);
		return alist.at(0).toString();
	}
	return "127.0.0.1";
}

/**
 * If no port is specified in the configuration, the default port is used.
 * @retval false if server could not be started
 * @post if returned true, server is now running in the background
 */
bool LocalServer::startServer()
{
	if(instance_==0)
		instance_ = new LocalServer();
	if(instance_->isRunning())
		return true;
	instance_->start();
	return true;
}

void LocalServer::stopServer()
{
	if(instance_ && instance_->isRunning()) {
		instance_->quit();
	}
}

void LocalServer::run() {
	server_ = new server::Server();
	int port;
	{
	QSettings& cfg = DrawPileApp::getSettings();
	cfg.beginGroup("settings/server");
	port = cfg.value("port", protocol::DEFAULT_PORT).toInt();
	server_->setUniqueIps(!cfg.value("multiconnect",true).toBool());
	server_->setMaxNameLength(cfg.value("maxnamelength",16).toInt());
	}
	connect(server_, SIGNAL(lastClientLeft()), this, SLOT(quit()));
	if(server_->start(port)==false) {
		qWarning() << "Couldn't start server.";
		return;
	}
	qDebug() << "server started on port" << port;

	exec();

	server_->stop();
	delete server_;
	qDebug() << "server stopped";
}

