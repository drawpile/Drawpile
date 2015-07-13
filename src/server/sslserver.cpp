/*
   Drawpile - a collaborative drawing program.

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

#include "sslserver.h"
#include "../shared/util/logger.h"

#include <QSslSocket>
#include <QSslCipher>
#include <QSslConfiguration>
#include <QFile>

namespace server {

SslServer::SslServer(const QString &certFile, const QString &keyFile, QObject *parent) :
	QTcpServer(parent)
{
	if(!QSslSocket::supportsSsl()) {
		logger::error() << "SSL support not available!";
		return;
	}

	QFile cert(certFile);
	if(!cert.open(QFile::ReadOnly)) {
		logger::error() << "Couldn't open certificate:" << cert.errorString();
		return;
	}

	QFile key(keyFile);
	if(!key.open(QFile::ReadOnly)) {
		logger::error() << "Couldn't open private key:" << key.errorString();
		return;
	}

	_cert = QSslCertificate(&cert);
	if(_cert.isNull())
		logger::error() << "Invalid certificate";

	_key = QSslKey(&key, QSsl::Rsa);
	if(_key.isNull())
		logger::error() << "Invalid private key";
}

void SslServer::requireForwardSecrecy()
{
	QList<QSslCipher> ciphers;

	QStringList methods {"DH", "ECDH"};

	for(const QSslCipher &cipher : QSslSocket::defaultCiphers()) {
		if(methods.contains(cipher.keyExchangeMethod())) {
			ciphers.append(cipher);
		}
	}

	if(ciphers.isEmpty())
		logger::warning() << "Forward secrecy not available!";
	else
		QSslSocket::setDefaultCiphers(ciphers);
}

bool SslServer::isValidCert() const
{
	return !_cert.isNull() && !_key.isNull();
}

void SslServer::incomingConnection(qintptr handle)
{
	QSslSocket *socket = new QSslSocket(this);
	socket->setSocketDescriptor(handle);
	socket->setLocalCertificate(_cert);
	socket->setPrivateKey(_key);

	QSslConfiguration sslconf = socket->sslConfiguration();
	sslconf.setSslOption(QSsl::SslOptionDisableCompression, false);
	socket->setSslConfiguration(sslconf);

	addPendingConnection(socket);
}

}
