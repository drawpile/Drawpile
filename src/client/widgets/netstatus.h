/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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
#ifndef NETSTATUS_H
#define NETSTATUS_H

#include "net/server.h"

#include <QWidget>
#include <QUrl>

class QLabel;
class QTimer;
class QProgressBar;

namespace widgets {

class PopupMessage;

/**
 * @brief Network connection status widget
 * This widget displays the current status of the connection with the server
 * and the address of the host.
 */
class NetStatus : public QWidget
{
Q_OBJECT
public:
	NetStatus(QWidget *parent);

	void setSecurityLevel(net::Server::Security level, const QSslCertificate &certificate);

public slots:
	void connectingToHost(const QString& address, int port);
	void loggedIn(const QUrl &sessionUrl);
	void hostDisconnecting();
	void hostDisconnected();

	void expectBytes(int count);
	void sendingBytes(int count);
	void bytesReceived(int count);
	void bytesSent(int count);

	void lagMeasured(qint64 lag);

	// this is used with QNetworkReplies
	void bytesReceived(qint64 received, qint64 total);

	void join(int id, const QString& user);
	void leave(const QString& user);

	//! This user was kicked off the session
	void kicked(const QString& user);

	void copyAddress();
	void copyUrl();

signals:
	//! A status message
	void statusMessage(const QString& message);

private slots:
	void discoverAddress();
	void externalIpDiscovered(const QString &ip);
	void showCertificate();

private:
	void message(const QString& msg);
	QString fullAddress() const;

	QProgressBar *_download;
	QProgressBar *_upload;

	QLabel *_label, *_icon, *_security;
	PopupMessage *_popup;
	QString _address;
	int _port;
	QUrl _sessionUrl;

	QAction *_copyaction;
	QAction *_urlaction;
	QAction *_discoverIp;

	quint64 _sentbytes, _recvbytes;

	QSslCertificate _certificate;
};

}

#endif

