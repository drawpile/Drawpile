// SPDX-License-Identifier: GPL-3.0-or-later

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
