/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2018 Calle Laakkonen

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
class SslServer final : public QTcpServer
{
	Q_OBJECT
public:
	SslServer(const QString &certFile, const QString &keyFile, QObject *parent = nullptr);

	bool isValidCert() const;

	/**
	 * @brief Limit QSslSocket's default ciphers to those that support forward secrecy
	 */
	static void requireForwardSecrecy();

protected:
	void incomingConnection(qintptr handle) override;

private:
	bool reloadCertChain();
	bool reloadKey();

	QList<QSslCertificate> m_certchain;
	QSslKey m_key;

	QString m_certPath;
	QString m_keyPath;
	QDateTime m_certLastMod;
	QDateTime m_keyLastMod;
};

}

#endif
