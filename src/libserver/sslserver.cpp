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

#include "libserver/sslserver.h"

#include <QSslSocket>
#include <QSslCipher>
#include <QSslConfiguration>
#include <QFile>
#include <QFileInfo>

namespace server {

SslServer::SslServer(const QString &certFile, const QString &keyFile, QObject *parent) :
	QTcpServer(parent), m_certPath(certFile), m_keyPath(keyFile)
{
	if(!QSslSocket::supportsSsl()) {
		qWarning("SSL support not available!");
		return;
	}

	reloadCertChain();
	reloadKey();
}

bool SslServer::reloadCertChain()
{
	QFileInfo fi(m_certPath);
	if(!fi.isReadable()) {
		qWarning("%s: certificate file not readable!", qPrintable(m_certPath));
		return false;
	}

	if(!m_certLastMod.isNull() && fi.lastModified() < m_certLastMod) {
		// Certificate file hasn't been modified
		return true;
	}

	QList<QSslCertificate> chain = QSslCertificate::fromPath(m_certPath);
	if(chain.isEmpty()) {
		qWarning("%s: invalid certificate", qPrintable(m_certPath));
		return false;
	}

	m_certLastMod = fi.lastModified();
	m_certchain = chain;
	return true;
}

bool SslServer::reloadKey()
{
	QFileInfo fi(m_keyPath);
	if(!fi.isReadable()) {
		qWarning("%s: key file not readable!", qPrintable(m_keyPath));
		return false;
	}

	if(!m_keyLastMod.isNull() && fi.lastModified() < m_keyLastMod) {
		// Key file hasn't been modified
		return true;
	}

	QFile keyfile(m_keyPath);
	if(!keyfile.open(QFile::ReadOnly)) {
		qWarning("%s: couldn't open private key: %s", qPrintable(m_keyPath), qPrintable(keyfile.errorString()));
		return false;
	}

	QSslKey key {&keyfile, QSsl::Rsa}; // TODO selectable algorithm
	if(key.isNull()) {
		qWarning("%s: Invalid private key", qPrintable(m_keyPath));
		return false;
	}

	m_keyLastMod = fi.lastModified();
	m_key = key;
	return true;
}

void SslServer::requireForwardSecrecy()
{
	QList<QSslCipher> ciphers;

	QStringList methods {"DH", "ECDH"};

	QSslConfiguration config = QSslConfiguration::defaultConfiguration();

	for(const auto &cipher : config.ciphers()) {
		if(methods.contains(cipher.keyExchangeMethod())) {
			ciphers.append(cipher);
		}
	}

	if(ciphers.isEmpty()) {
		qWarning("Forward secrecy not available!");

	} else {
		config.setCiphers(ciphers);
		QSslConfiguration::setDefaultConfiguration(config);
	}
}

bool SslServer::isValidCert() const
{
	return !m_certchain.isEmpty() && !m_key.isNull();
}

void SslServer::incomingConnection(qintptr handle)
{
	reloadCertChain();
	reloadKey();
	if(!isValidCert()) {
		qWarning("SSL not available for new connection!");
		QTcpServer::incomingConnection(handle);
		return;
	}

	QSslSocket *socket = new QSslSocket(this);
	socket->setSocketDescriptor(handle);
	socket->setLocalCertificateChain(m_certchain);
	socket->setPrivateKey(m_key);

	QSslConfiguration sslconf = socket->sslConfiguration();
	sslconf.setSslOption(QSsl::SslOptionDisableCompression, false);
	socket->setSslConfiguration(sslconf);

	addPendingConnection(socket);
}

}
