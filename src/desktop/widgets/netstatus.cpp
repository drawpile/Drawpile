// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/main.h"
#include "desktop/widgets/netstatus.h"
#include "desktop/widgets/popupmessage.h"
#include "desktop/dialogs/certificateview.h"
#include "desktop/dialogs/netstats.h"
#include "libshared/util/whatismyip.h"

#include <QAction>
#include <QIcon>
#include <QLabel>
#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QProgressBar>
#include <QMessageBox>
#include <QCheckBox>
#include <QTimer>

namespace widgets {

NetStatus::NetStatus(QWidget *parent)
	: QWidget(parent), m_state(NotConnected), _sentbytes(0), _recvbytes(0), _lag(0)
{
	setMinimumHeight(16+2);

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
			Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard
			);
	m_label->setCursor(Qt::IBeamCursor);
	m_label->setContextMenuPolicy(Qt::ActionsContextMenu);
	layout->addWidget(m_label);

	// Action to copy address to clipboard
	_copyaction = new QAction(tr("Copy address to clipboard"), this);
	_copyaction->setEnabled(false);
	m_label->addAction(_copyaction);
	connect(_copyaction,SIGNAL(triggered()),this,SLOT(copyAddress()));

	// Action to copy the full session URL to clipboard
	_urlaction = new QAction(tr("Copy session URL to clipboard"), this);
	_urlaction->setEnabled(false);
	m_label->addAction(_urlaction);
	connect(_urlaction, SIGNAL(triggered()), this, SLOT(copyUrl()));

	// Discover local IP address
	_discoverIp = new QAction(tr("Get externally visible IP address"), this);
	_discoverIp->setVisible(false);
	m_label->addAction(_discoverIp);
	connect(_discoverIp, SIGNAL(triggered()), this, SLOT(discoverAddress()));
	connect(WhatIsMyIp::instance(), SIGNAL(myAddressIs(QString)), this, SLOT(externalIpDiscovered(QString)));

	// Option to hide the server address
	// (useful when livestreaming)
	QAction *hideServerAction = new QAction(tr("Hide address"), this);
	hideServerAction->setCheckable(true);

	auto &settings = dpApp().settings();
	settings.bindServerHideIp(hideServerAction, [=](bool hide) {
		hideServerAction->setChecked(hide);
		m_hideServer = hide;
		updateLabel();
	}, &QAction::triggered);
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

	// Some styles are buggy and have bad tooltip colors, so we force the colors here.
	QPalette popupPalette;
	popupPalette.setColor(QPalette::ToolTipBase, Qt::black);
	popupPalette.setColor(QPalette::ToolTipText, Qt::white);
	m_popup->setPalette(popupPalette);

	updateLabel();
}

/**
 * Set the label to display the address.
 * A context menu to copy the address to clipboard will be enabled.
 * @param address the address to display
 */
void NetStatus::connectingToHost(const QString& address, int port)
{
	m_address = address;
	m_port = port;
	m_state = Connecting;
	_copyaction->setEnabled(true);
	updateLabel();
	message(m_label->text());

	// Enable "discover IP" item for local host
	bool isLocal = WhatIsMyIp::isMyPrivateAddress(address);
	_discoverIp->setEnabled(isLocal);
	_discoverIp->setVisible(isLocal);

	if(!isLocal && WhatIsMyIp::isCGNAddress(address))
		showCGNAlert();

	// reset statistics
	_recvbytes = 0;
	_sentbytes = 0;
}

void NetStatus::loggedIn(const QUrl &sessionUrl)
{
	m_sessionUrl = sessionUrl;
	_urlaction->setEnabled(true);
	m_state = LoggedIn;
	updateLabel();
	message(tr("Logged in!"));
	if(_netstats)
		_netstats->setCurrentLag(_lag);
}

void NetStatus::setRoomcode(const QString &roomcode)
{
	m_roomcode = roomcode;
	updateLabel();
}

void NetStatus::setSecurityLevel(net::Server::Security level, const QSslCertificate &certificate)
{
	QString iconname;
	QString tooltip;
	switch(level) {
	case net::Server::NO_SECURITY: break;
	case net::Server::NEW_HOST:
		iconname = "security-medium";
		tooltip = tr("A previously unvisited host");
		break;

	case net::Server::KNOWN_HOST:
		iconname = "security-medium";
		tooltip = tr("Host certificate has not changed since the last visit");
		break;

	case net::Server::TRUSTED_HOST:
		iconname = "security-high";
		tooltip = tr("This is a trusted host");
		break;
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
	message(m_label->text());
}

/**
 * Set the label to indicate a lack of connection.
 * Context menu will be disabled.
 */
void NetStatus::hostDisconnected()
{
	m_address = QString();
	m_roomcode = QString();
	m_state = NotConnected;
	updateLabel();

	_urlaction->setEnabled(false);
	_copyaction->setEnabled(false);
	_discoverIp->setVisible(false);

	message(tr("Disconnected"));
	setSecurityLevel(net::Server::NO_SECURITY, QSslCertificate());

	if(_netstats)
		_netstats->setDisconnected();
}

void NetStatus::bytesReceived(int count)
{
	_recvbytes += count;
	if(_netstats)
		_netstats->setRecvBytes(_recvbytes);
}

void NetStatus::setCatchupProgress(int progress)
{
	if(progress<100) {
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
	_sentbytes += count;

	if(_netstats)
		_netstats->setSentBytes(_recvbytes);
}

void NetStatus::lagMeasured(qint64 lag)
{
	_lag = lag;
	if(_netstats)
		_netstats->setCurrentLag(lag);
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
	_discoverIp->setEnabled(false);
}

void NetStatus::externalIpDiscovered(const QString &ip)
{
	// Only update IP if solicited
	if(_discoverIp->isVisible()) {
		_discoverIp->setEnabled(false);

		// TODO handle IPv6 style addresses
		int portsep = m_address.lastIndexOf(':');
		QString port;
		if(portsep>0)
			port = m_address.mid(portsep);

		m_address = ip;
		m_sessionUrl.setHost(ip);
		updateLabel();

		if(WhatIsMyIp::isCGNAddress(ip))
			showCGNAlert();
	}
}

QString NetStatus::fullAddress() const
{
	QString addr;
	if(m_port>0)
		addr = QString("%1:%2").arg(m_address).arg(m_port);
	else
		addr = m_address;

	return addr;
}

void NetStatus::kicked(const QString& user)
{
	message(tr("You have been kicked by %1").arg(user.toHtmlEscaped()));
}

void NetStatus::message(const QString &msg)
{
	m_popup->showMessage(
				mapToGlobal(m_label->pos() + QPoint(m_label->width()/2, 2)),
				msg);
}

void NetStatus::updateLabel()
{
	QString txt;
	switch(m_state) {
	case NotConnected: txt = tr("not connected"); break;
	case Connecting:
		if(m_hideServer)
			txt = tr("Connecting...");
		else
			txt = tr("Connecting to %1...").arg(fullAddress());
		break;
	case LoggedIn:
		if(m_hideServer)
			txt = tr("Connected");
		else if(m_roomcode.isEmpty())
			txt = tr("Host: %1").arg(fullAddress());
		else
			txt = tr("Room: %1").arg(m_roomcode);
		break;
	case Disconnecting: txt = tr("Logging out..."); break;
	}
	m_label->setText(txt);
}

void NetStatus::showCertificate()
{
	if(!m_certificate)
		return;
	dialogs::CertificateView *certdlg = new dialogs::CertificateView(m_address, *m_certificate, parentWidget());
	certdlg->setAttribute(Qt::WA_DeleteOnClose);
	certdlg->show();
}

void NetStatus::showNetStats()
{
	if(!_netstats) {
		_netstats = new dialogs::NetStats(this);
		_netstats->setWindowFlags(Qt::Tool);
		_netstats->setAttribute(Qt::WA_DeleteOnClose);

		_netstats->setRecvBytes(_recvbytes);
		_netstats->setSentBytes(_sentbytes);
		if(!m_address.isEmpty())
			_netstats->setCurrentLag(_lag);
	}
	_netstats->show();
}

void NetStatus::showCGNAlert()
{
	auto &settings = dpApp().settings();

	if(!settings.ignoreCarrierGradeNat()) {
		QMessageBox box(
			QMessageBox::Warning,
			tr("Notice"),
			tr("Your Internet Service Provider is using Carrier Grade NAT. This makes it impossible for others to connect to you directly. See Drawpile's help page for workarounds."),
			QMessageBox::Ok
		);

		box.setCheckBox(new QCheckBox(tr("Don't show this again")));
		box.exec();

		settings.setIgnoreCarrierGradeNat(box.checkBox()->isChecked());
	}
}

}

