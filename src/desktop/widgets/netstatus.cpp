// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/widgets/netstatus.h"
#include "desktop/dialogs/certificateview.h"
#include "desktop/dialogs/netstats.h"
#include "desktop/main.h"
#include "desktop/widgets/popupmessage.h"
#include "libshared/util/whatismyip.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QTimer>
#include <QVBoxLayout>

namespace widgets {

NetStatus::NetStatus(QWidget *parent)
	: QWidget(parent)
	, m_state(NotConnected)
	, m_haveJoinPassword(false)
	, m_isLocalHost(false)
	, m_haveRemoteAddress(false)
	, m_sentbytes(0)
	, m_recvbytes(0)
	, m_lag(0)
{
	setMinimumHeight(16 + 2);

	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setContentsMargins(1, 1, 1, 1);
	layout->setSpacing(4);

	// Download progress bar
	m_download = new QProgressBar(this);
	m_download->setMaximumWidth(120);
	m_download->setSizePolicy(QSizePolicy());
	m_download->setTextVisible(false);
	m_download->setMaximum(100);
	m_download->hide();
	layout->addWidget(m_download);

	// Host address label
	m_label = new QLabel(this);
	m_label->setTextInteractionFlags(
		Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	m_label->setCursor(Qt::IBeamCursor);
	m_label->setContextMenuPolicy(Qt::ActionsContextMenu);
	layout->addWidget(m_label);

	// Action to copy address to clipboard
	m_copyaction = new QAction(tr("Copy address to clipboard"), this);
	m_copyaction->setEnabled(false);
	m_label->addAction(m_copyaction);
	connect(m_copyaction, SIGNAL(triggered()), this, SLOT(copyAddress()));

	// Action to copy the full session URL to clipboard
	m_urlaction = new QAction(tr("Copy session URL to clipboard"), this);
	m_urlaction->setEnabled(false);
	m_label->addAction(m_urlaction);
	connect(m_urlaction, SIGNAL(triggered()), this, SLOT(copyUrl()));

	// Discover local IP address
	m_discoverIp = new QAction(tr("Get externally visible IP address"), this);
	m_discoverIp->setVisible(false);
	m_label->addAction(m_discoverIp);
	connect(m_discoverIp, SIGNAL(triggered()), this, SLOT(discoverAddress()));
	connect(
		WhatIsMyIp::instance(), SIGNAL(myAddressIs(QString)), this,
		SLOT(externalIpDiscovered(QString)));

	// Option to hide the server address
	// (useful when livestreaming)
	QAction *hideServerAction = new QAction(tr("Hide address"), this);
	hideServerAction->setCheckable(true);

	auto &settings = dpApp().settings();
	settings.bindServerHideIp(
		hideServerAction,
		[=](bool hide) {
			hideServerAction->setChecked(hide);
			m_hideServer = hide;
			updateLabel();
		},
		&QAction::triggered);
	m_label->addAction(hideServerAction);

	// Show network statistics
	QAction *sep = new QAction(this);
	sep->setSeparator(true);
	m_label->addAction(sep);

	QAction *showNetStats = new QAction(tr("Statistics"), this);
	m_label->addAction(showNetStats);
	connect(showNetStats, SIGNAL(triggered()), this, SLOT(showNetStats()));

	// Security level icon
	m_security = new QLabel(QString(), this);
	m_security->setFixedSize(QSize(16, 16));
	m_security->hide();
	layout->addWidget(m_security);

	m_security->setContextMenuPolicy(Qt::ActionsContextMenu);

	QAction *showcert = new QAction(tr("Show certificate"), this);
	m_security->addAction(showcert);
	connect(showcert, SIGNAL(triggered()), this, SLOT(showCertificate()));

	// Popup label
	m_popup = new PopupMessage(this);

	// Some styles are buggy and have bad tooltip colors, so we force the colors
	// here.
	QPalette popupPalette;
	popupPalette.setColor(QPalette::ToolTipBase, Qt::black);
	popupPalette.setColor(QPalette::ToolTipText, Qt::white);
	m_popup->setPalette(popupPalette);

	updateLabel();
}

void NetStatus::setHaveJoinPassword(bool haveJoinPassword)
{
	if(haveJoinPassword != m_haveJoinPassword) {
		m_haveJoinPassword = haveJoinPassword;
		emit joinPasswordChanged();
	}
}

void NetStatus::setJoinPassword(const QString &joinPassword)
{
	if(joinPassword != m_joinPassword) {
		m_joinPassword = joinPassword;
		emit joinPasswordChanged();
	}
}

/**
 * Set the label to display the address.
 * A context menu to copy the address to clipboard will be enabled.
 * @param address the address to display
 */
void NetStatus::connectingToHost(const QString &address, int port)
{
	m_address = address;
	m_port = port;
	m_state = Connecting;
	m_copyaction->setEnabled(true);
	updateLabel();

	// Enable "discover IP" item for local host
	m_isLocalHost = WhatIsMyIp::isMyPrivateAddress(address);
	m_haveRemoteAddress = !m_isLocalHost;
	m_discoverIp->setEnabled(m_isLocalHost);
	m_discoverIp->setVisible(m_isLocalHost);

	if(!m_isLocalHost && WhatIsMyIp::isCGNAddress(address))
		showCGNAlert();

	// reset statistics
	m_recvbytes = 0;
	m_sentbytes = 0;
}

void NetStatus::loggedIn(const QUrl &sessionUrl, const QString &joinPassword)
{
	m_sessionUrl = sessionUrl;
	m_joinPassword = joinPassword;
	m_haveJoinPassword = !joinPassword.isEmpty();
	m_urlaction->setEnabled(true);
	m_state = LoggedIn;
	updateLabel();
	if(m_netstats && m_netstats->isVisible()) {
		m_netstats->setCurrentLag(m_lag);
	}
}

void NetStatus::setSecurityLevel(
	net::Server::Security level, const QSslCertificate &certificate,
	bool isSelfSigned)
{
	QString iconname;
	QString tooltip;
	if(isSelfSigned) {
		switch(level) {
		case net::Server::NO_SECURITY:
			break;

		case net::Server::NEW_HOST:
			iconname = "security-medium";
			tooltip = tr("Self-signed certificate");
			break;

		case net::Server::KNOWN_HOST:
			iconname = "security-medium";
			tooltip = tr("Known self-signed certificate");
			break;

		case net::Server::TRUSTED_HOST:
			iconname = "security-high";
			tooltip = tr("Pinned self-signed certificate");
			break;
		}
	} else {
		switch(level) {
		case net::Server::NO_SECURITY:
			break;

		case net::Server::NEW_HOST:
		case net::Server::KNOWN_HOST:
			iconname = "security-high";
			tooltip = tr("Valid certificate");
			break;

		case net::Server::TRUSTED_HOST:
			iconname = "security-high";
			tooltip = tr("Pinned valid certificate");
			break;
		}
	}

	if(iconname.isEmpty()) {
		m_security->hide();
	} else {
		m_security->setPixmap(QIcon::fromTheme(iconname).pixmap(16, 16));
		m_security->setToolTip(tooltip);
		m_security->show();
	}

	m_certificate.reset(new QSslCertificate(certificate));
}

void NetStatus::hostDisconnecting()
{
	m_state = Disconnecting;
	updateLabel();
}

/**
 * Set the label to indicate a lack of connection.
 * Context menu will be disabled.
 */
void NetStatus::hostDisconnected()
{
	m_address = QString();
	m_isLocalHost = false;
	m_haveRemoteAddress = false;
	m_joinPassword = QString();
	m_state = NotConnected;
	updateLabel();

	m_urlaction->setEnabled(false);
	m_copyaction->setEnabled(false);
	m_discoverIp->setVisible(false);

	setSecurityLevel(net::Server::NO_SECURITY, QSslCertificate(), false);

	if(m_netstats)
		m_netstats->setDisconnected();
}

void NetStatus::bytesReceived(int count)
{
	m_recvbytes += count;
	if(m_netstats && m_netstats->isVisible()) {
		m_netstats->setRecvBytes(m_recvbytes);
	}
}

void NetStatus::setCatchupProgress(int progress)
{
	if(progress < 100) {
		m_download->show();
		m_download->setValue(progress);
	} else {
		hideDownloadProgress();
	}
}

void NetStatus::setDownloadProgress(qint64 received, qint64 total)
{
	if(received < total) {
		m_download->show();
		int progress = 100 * received / total;
		m_download->setValue(progress);
	} else {
		hideDownloadProgress();
	}
}

void NetStatus::hideDownloadProgress()
{
	m_download->hide();
}

void NetStatus::bytesSent(int count)
{
	m_sentbytes += count;

	if(m_netstats && m_netstats->isVisible()) {
		m_netstats->setSentBytes(m_recvbytes);
	}
}

void NetStatus::lagMeasured(qint64 lag)
{
	m_lag = lag;
	if(m_netstats && m_netstats->isVisible()) {
		m_netstats->setCurrentLag(lag);
	}
}

/**
 * Copy the current address to clipboard.
 * Should not be called if disconnected.
 */
void NetStatus::copyAddress()
{
	QString addr = fullAddress();
	QApplication::clipboard()->setText(addr);
	// Put address also in selection buffer so it can be pasted with
	// a middle mouse click where supported.
	QApplication::clipboard()->setText(addr, QClipboard::Selection);
}

void NetStatus::copyUrl()
{
	QString url = m_sessionUrl.toString();
	QApplication::clipboard()->setText(url);
	QApplication::clipboard()->setText(url, QClipboard::Selection);
}

void NetStatus::discoverAddress()
{
	WhatIsMyIp::instance()->discoverMyIp();
	m_discoverIp->setEnabled(false);
}

void NetStatus::externalIpDiscovered(const QString &ip)
{
	// Only update IP if solicited
	if(m_discoverIp->isVisible()) {
		m_discoverIp->setEnabled(false);

		int port = m_sessionUrl.port();
		m_address = ip;
		m_haveRemoteAddress = true;
		// QUrl doesn't seem to handle IPv6 without ports correctly.
		if(ip.contains(':') && port <= 0) {
			m_sessionUrl.setPort(cmake_config::proto::port());
		}
		m_sessionUrl.setHost(ip);
		updateLabel();

		if(WhatIsMyIp::isCGNAddress(ip))
			showCGNAlert();

		emit remoteAddressDiscovered();
	}
}

QString NetStatus::fullAddress() const
{
	QString addr;
	if(m_port > 0)
		addr = QString("%1:%2").arg(m_address).arg(m_port);
	else
		addr = m_address;

	return addr;
}

void NetStatus::showMessage(const QString &msg)
{
	m_popup->showMessage(
		mapToGlobal(m_label->pos() + QPoint(m_label->width() / 2, 2)), msg);
}

void NetStatus::updateLabel()
{
	QString txt;
	switch(m_state) {
	case NotConnected:
		txt = tr("not connected");
		break;
	case Connecting:
		if(m_hideServer)
			txt = tr("Connecting...");
		else
			txt = tr("Connecting to %1...").arg(fullAddress());
		break;
	case LoggedIn:
		if(m_hideServer)
			txt = tr("Connected");
		else
			txt = tr("Host: %1").arg(fullAddress());
		break;
	case Disconnecting:
		txt = tr("Logging out...");
		break;
	}
	m_label->setText(txt);
}

void NetStatus::showCertificate()
{
	if(!m_certificate)
		return;
	dialogs::CertificateView *certdlg =
		new dialogs::CertificateView(m_address, *m_certificate, parentWidget());
	certdlg->setAttribute(Qt::WA_DeleteOnClose);
	certdlg->show();
}

void NetStatus::showNetStats()
{
	if(!m_netstats) {
		m_netstats = new dialogs::NetStats(this);
		m_netstats->setWindowFlags(Qt::Tool);
		m_netstats->setAttribute(Qt::WA_DeleteOnClose);
	}
	m_netstats->setRecvBytes(m_recvbytes);
	m_netstats->setSentBytes(m_sentbytes);
	if(!m_address.isEmpty()) {
		m_netstats->setCurrentLag(m_lag);
	}
	m_netstats->updateMemoryUsage();
	m_netstats->show();
}

void NetStatus::showCGNAlert()
{
	auto &settings = dpApp().settings();

	if(!settings.ignoreCarrierGradeNat()) {
		QMessageBox box(
			QMessageBox::Warning, tr("Notice"),
			tr("Your Internet Service Provider is using Carrier Grade NAT. "
			   "This makes it impossible for others to connect to you "
			   "directly. See Drawpile's help page for workarounds."),
			QMessageBox::Ok);

		box.setCheckBox(new QCheckBox(tr("Don't show this again")));
		box.exec();

		settings.setIgnoreCarrierGradeNat(box.checkBox()->isChecked());
	}
}

}
