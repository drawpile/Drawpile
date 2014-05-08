/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
#ifndef SSLSERVER_H
#define SSLSERVER_H

#include <QTcpServer>
#include <QSslCertificate>
#include <QSslKey>

namespace server {

/**
 * @brief A TcpServer subclass that creates QSslSockets instead of QTcpSockets
 */
class SslServer : public QTcpServer
{
	Q_OBJECT
public:
	SslServer(const QString &certFile, const QString &keyFile, QObject *parent = 0);

	bool isValidCert() const;

protected:
	void incomingConnection(qintptr handle);

private:
	QSslCertificate _cert;
	QSslKey _key;
};

}

#endif
