// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBSERVER_SSLSERVER_H
#define LIBSERVER_SSLSERVER_H
#include <QSslCertificate>
#include <QSslKey>
#include <QTcpServer>

namespace server {

/**
 * @brief A TcpServer subclass that creates QSslSockets instead of QTcpSockets
 */
class SslServer final : public QTcpServer {
	Q_OBJECT
public:
	enum class Algorithm { Guess, Rsa, Dsa, Ec, Dh };

	SslServer(
		const QString &certFile, const QString &keyFile, Algorithm keyAlgorithm,
		QObject *parent = nullptr);

	bool isValidCert() const;

protected:
	void incomingConnection(qintptr handle) override;

private:
	bool reloadCertChain();
	bool reloadKey();
	QSslKey loadKey(const QByteArray &encoded) const;
	const char *getKeyAlgorithmDescription() const;

	QList<QSslCertificate> m_certchain;
	QSslKey m_key;

	QString m_certPath;
	QString m_keyPath;
	Algorithm m_keyAlgorithm;
	QDateTime m_certLastMod;
	QDateTime m_keyLastMod;
};

}

#endif
