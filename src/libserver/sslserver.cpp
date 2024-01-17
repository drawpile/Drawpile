// SPDX-License-Identifier: GPL-3.0-or-later
#include "libserver/sslserver.h"
#include <QFile>
#include <QFileInfo>
#include <QSslCipher>
#include <QSslConfiguration>
#include <QSslSocket>

namespace server {

SslServer::SslServer(
	const QString &certFile, const QString &keyFile, Algorithm keyAlgorithm,
	QObject *parent)
	: QTcpServer(parent)
	, m_certPath(certFile)
	, m_keyPath(keyFile)
	, m_keyAlgorithm(keyAlgorithm)
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
		qWarning(
			"%s: certificate file not readable!", qUtf8Printable(m_certPath));
		return false;
	}

	if(!m_certLastMod.isNull() && fi.lastModified() < m_certLastMod) {
		// Certificate file hasn't been modified
		return true;
	}

	QList<QSslCertificate> chain = QSslCertificate::fromPath(m_certPath);
	if(chain.isEmpty()) {
		qWarning("%s: invalid certificate", qUtf8Printable(m_certPath));
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
		qWarning("%s: key file not readable!", qUtf8Printable(m_keyPath));
		return false;
	}

	if(!m_keyLastMod.isNull() && fi.lastModified() < m_keyLastMod) {
		// Key file hasn't been modified
		return true;
	}

	QByteArray encoded;
	{
		QFile keyfile(m_keyPath);
		if(!keyfile.open(QFile::ReadOnly)) {
			qWarning(
				"%s: couldn't open private key: %s", qUtf8Printable(m_keyPath),
				qUtf8Printable(keyfile.errorString()));
			return false;
		}

		keyfile.unsetError();
		encoded = keyfile.readAll();
		if(keyfile.error() != QFileDevice::NoError) {
			qWarning(
				"%s: couldn't read private key: %s", qUtf8Printable(m_keyPath),
				qUtf8Printable(keyfile.errorString()));
			return false;
		}
	}

	QSslKey key = loadKey(encoded);
	if(key.isNull()) {
		qWarning(
			"%s: couldn't load private key via algorithm %s",
			qUtf8Printable(m_keyPath), getKeyAlgorithmDescription());
		return false;
	}

	m_keyLastMod = fi.lastModified();
	m_key = key;
	return true;
}

QSslKey SslServer::loadKey(const QByteArray &encoded) const
{
	QSslKey key;
	switch(m_keyAlgorithm) {
	case Algorithm::Guess:
		key = QSslKey(encoded, QSsl::Rsa);
		if(key.isNull()) {
			key = QSslKey(encoded, QSsl::Ec);
		}
		break;
	case Algorithm::Rsa:
		key = QSslKey(encoded, QSsl::Rsa);
		break;
	case Algorithm::Dsa:
		key = QSslKey(encoded, QSsl::Dsa);
		break;
	case Algorithm::Ec:
		key = QSslKey(encoded, QSsl::Ec);
		break;
	case Algorithm::Dh:
		key = QSslKey(encoded, QSsl::Dh);
		break;
	}
	return key;
}

const char *SslServer::getKeyAlgorithmDescription() const
{
	switch(m_keyAlgorithm) {
	case Algorithm::Guess:
		return "guess (tried rsa and ec)";
	case Algorithm::Rsa:
		return "rsa";
	case Algorithm::Dsa:
		return "dsa";
	case Algorithm::Ec:
		return "ec";
	case Algorithm::Dh:
		return "dh";
	}
	return "unknown";
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
