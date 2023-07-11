// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NETSTATUS_H
#define NETSTATUS_H

#include "libclient/net/server.h"

#include <QWidget>
#include <QUrl>
#include <QPointer>
#include <QSslCertificate>

class QLabel;
class QTimer;
class QProgressBar;

namespace dialogs {
	class NetStats;
}

namespace widgets {

class PopupMessage;

/**
 * @brief Network connection status widget
 * This widget displays the current status of the connection with the server
 * and the address of the host.
 */
class NetStatus final : public QWidget
{
Q_OBJECT
public:
	NetStatus(QWidget *parent);

	void setSecurityLevel(net::Server::Security level, const QSslCertificate &certificate);

	bool isLocalHost() const { return m_isLocalHost; }
	bool haveRemoteAddress() const { return m_haveRemoteAddress; }
	const QUrl &sessionUrl() const { return m_sessionUrl; }
	QString joinPassword() const { return m_haveJoinPassword ? m_joinPassword : QString{}; }

signals:
	void remoteAddressDiscovered();

public slots:
	void setHaveJoinPassword(bool haveJoinPassword);
	void setJoinPassword(const QString &joinPassword);
	void connectingToHost(const QString& address, int port);
	void loggedIn(const QUrl &sessionUrl, const QString &joinPassword);
	void hostDisconnecting();
	void hostDisconnected();

	void setRoomcode(const QString &roomcode);

	void bytesReceived(int count);
	void bytesSent(int count);

	void lagMeasured(qint64 lag);

	//! Update the download progress bar with message catchup progress (0-100)
	void setCatchupProgress(int progress);

	//! Update the download progress bar with file download progress (0-total)
	void setDownloadProgress(qint64 received, qint64 total);

	//! Download over, hide the progress bar
	void hideDownloadProgress();

	//! This user was kicked off the session
	void kicked(const QString& user);

	void copyAddress();
	void copyUrl();
	void message(const QString &msg);

	void discoverAddress();

private slots:
	void externalIpDiscovered(const QString &ip);
	void showCertificate();
	void showNetStats();

private:
	void showCGNAlert();
	void updateLabel();

	enum { NotConnected, Connecting, LoggedIn, Disconnecting } m_state;

	QString fullAddress() const;

	QPointer<dialogs::NetStats> _netstats;
	QProgressBar *m_download;

	QLabel *m_label, *m_security;
	PopupMessage *m_popup;
	QString m_address;
	QString m_roomcode;
	int m_port;
	QUrl m_sessionUrl;
	QString m_joinPassword;
	bool m_haveJoinPassword;

	bool m_hideServer;
	bool m_isLocalHost;
	bool m_haveRemoteAddress;

	QAction *_copyaction;
	QAction *_urlaction;
	QAction *_discoverIp;

	quint64 _sentbytes, _recvbytes, _lag;

	QScopedPointer<const QSslCertificate> m_certificate;
};

}

#endif

