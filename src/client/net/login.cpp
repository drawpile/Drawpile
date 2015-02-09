/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

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

#include "config.h"

#include "dialogs/selectsessiondialog.h"
#include "dialogs/logindialog.h"
#include "net/login.h"
#include "net/loginsessions.h"
#include "net/tcpserver.h"

#include "../shared/net/login.h"
#include "../shared/net/meta.h"

#include <QDebug>
#include <QStringList>
#include <QInputDialog>
#include <QRegularExpression>
#include <QSslSocket>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QHostAddress>
#include <QMessageBox>

//#define DEBUG_LOGIN

namespace {

enum CertLocation { KNOWN_HOSTS, TRUSTED_HOSTS };
QFileInfo getCertFile(CertLocation location, const QString &hostname)
{
	QString locationpath;
	if(location == KNOWN_HOSTS)
		locationpath = "/known-hosts/";
	else
		locationpath = "/trusted-hosts/";

	QDir certDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + locationpath);
	certDir.mkpath(".");

	return QFileInfo(certDir, hostname + ".pem");
}

}

namespace net {

LoginHandler::LoginHandler(Mode mode, const QUrl &url, QWidget *parent)
	: QObject(parent),
	  _mode(mode),
	  _address(url),
	  _widgetParent(parent),
	  _maxusers(0),
	  _allowdrawing(true),
	  _layerctrllock(true),
	  _state(EXPECT_HELLO),
	  _multisession(false),
	  _tls(false)
{
	_sessions = new LoginSessionModel(this);

	// Automatically join a session if the ID is included in the URL
	QString path = _address.path();
	if(path.length()>1) {
		QRegularExpression idre("\\A/([a-zA-Z0-9:-]{1,64})/?\\z");
		auto m = idre.match(path);
		if(m.hasMatch())
			_autoJoinId = m.captured(1);
	}
}

void LoginHandler::serverDisconnected()
{
	if(_selectorDialog)
		_selectorDialog->deleteLater();
	if(_passwordDialog)
		_passwordDialog->deleteLater();
	if(_certDialog)
		_certDialog->deleteLater();
}

void LoginHandler::receiveMessage(protocol::MessagePtr message)
{
	if(message->type() == protocol::MSG_DISCONNECT) {
		// server reports login errors with MSG_LOGIN, so there is nothing
		// of more interest here.
		return;
	}

	if(message->type() != protocol::MSG_LOGIN) {
		qWarning() << "Login error: got message type" << message->type() << "when expected type 0";
		failLogin(tr("Invalid state"));
		return;
	}

	QString msg = message.cast<protocol::Login>().message();

#ifdef DEBUG_LOGIN
	qDebug() << "login <--" << msg;
#endif

	// Overall, the login process is:
	// 1. wait for server greeting
	// 2. Upgrade to secure connection (if available)
	// 3. Authenticate user (or do guest login)
	// 4. get hosting password from user if needed
	// 5. wait for session list
	// 6. wait for user to finish typing host/join password if needed
	// 7. send host/join command
	// 8. wait for OK

	if(msg.startsWith("ERROR")) {
		// The server disconnects us right after sending the error message
		handleError(msg);
		return;
	}

	switch(_state) {
	case EXPECT_HELLO: expectHello(msg); break;
	case EXPECT_STARTTLS: expectStartTls(msg); break;
	case WAIT_FOR_LOGIN_PASSWORD: expectNothing(msg); break;
	case EXPECT_IDENTIFIED: expectIdentified(msg); break;
	case EXPECT_SESSIONLIST_TO_JOIN: expectSessionDescriptionJoin(msg); break;
	case EXPECT_SESSIONLIST_TO_HOST: expectSessionDescriptionHost(msg); break;
	case WAIT_FOR_JOIN_PASSWORD:
	case WAIT_FOR_HOST_PASSWORD:
	case EXPECT_LOGIN_OK: expectLoginOk(msg); break;
	case ABORT_LOGIN: /* ignore messages in this state */ break;
	}
}

void LoginHandler::expectNothing(const QString &msg)
{
	qWarning() << "Got login message" << msg << "while not expecting anything!";
	failLogin(tr("Incompatible server"));
}

void LoginHandler::expectHello(const QString &msg)
{
	// Greeting should be in format "DRAWPILE <majorVersion> <flags>
	const QRegularExpression re("\\ADRAWPILE (\\d+) (-|[\\w,]+)\\z");
	auto m = re.match(msg);

	if(!m.hasMatch()) {
		qWarning() << "Login error. Invalid greeting:" << msg;
		failLogin(tr("Incompatible server"));
		return;
	}

	// Major version must match ours
	int majorVersion = m.captured(1).toInt();
	if(majorVersion != DRAWPILE_PROTO_MAJOR_VERSION) {
		failLogin(tr("Server is for a different Drawpile version!"));
		return;
	}

	// Parse server capability flags
	QStringList flags = m.captured(2).split(",");
	bool mustSecure = false;
	_needHostPassword = false;
	_canAuth = false;
	_mustAuth = false;
	_needUserPassword = false;
	for(const QString &flag : flags) {
		if(flag == "-") {
			// no flags
		} else if(flag == "MULTI") {
			_multisession = true;
		} else if(flag == "HOSTP") {
			_needHostPassword = true;
		} else if(flag == "TLS") {
			_tls = true;
		} else if(flag == "SECURE") {
			mustSecure = true;
		} else if(flag == "PERSIST") {
			// TODO indicate persistent session support
		} else if(flag == "IDENT") {
			_canAuth = true;
		} else if(flag == "NOGUEST") {
			_mustAuth = true;
		} else {
			qWarning() << "Unknown server capability:" << flag;
		}
	}

	// Start secure mode if possible
	if(QSslSocket::supportsSsl() && _tls) {
		_state = EXPECT_STARTTLS;
		send("STARTTLS");

	} else {
		// If this is a trusted host, it should always be in secure mode
		if(QSslSocket::supportsSsl() && getCertFile(TRUSTED_HOSTS, _address.host()).exists()) {
			failLogin(tr("Secure mode not enabled on a trusted host!"));
			return;
		}

		if(mustSecure) {
			failLogin(tr("This is a secure secure server, but secure connection support is not available!"));
			return;
		}

		_tls = false;
		prepareToSendIdentity();
	}
}

void LoginHandler::expectStartTls(const QString &msg)
{
	Q_ASSERT(_tls);
	if(msg == "STARTTLS") {
		startTls();

	} else {
		qWarning() << "Login error. Expected STARTTLS, got:" << msg;
		failLogin(tr("Incompatible server"));
	}
}

void LoginHandler::showPasswordDialog(const QString &title, const QString &text)
{
	Q_ASSERT(_passwordDialog.isNull());

	_passwordDialog = new dialogs::LoginDialog(_widgetParent);

	_passwordDialog->setWindowModality(Qt::WindowModal);
	_passwordDialog->setWindowTitle(title);
	_passwordDialog->setIntroText(text);
	_passwordDialog->setUsername(_address.userName(), false);

	connect(_passwordDialog, SIGNAL(login(QString,QString)), this, SLOT(passwordSet(QString)));
	connect(_passwordDialog, SIGNAL(rejected()), this, SLOT(cancelLogin()));

	_passwordDialog->show();
}

void LoginHandler::passwordSet(const QString &password)
{
	Q_ASSERT(!_passwordDialog.isNull());

	switch(_state) {
	case WAIT_FOR_HOST_PASSWORD:
		_hostPassword = password;
		sendHostCommand();
		break;
	case WAIT_FOR_JOIN_PASSWORD:
		_joinPassword = password;
		sendJoinCommand();
		break;
	default:
		// shouldn't happen...
		qFatal("invalid state");
		return;
	}
}

void LoginHandler::prepareToSendIdentity()
{
	if(_mustAuth || _needUserPassword) {
		dialogs::LoginDialog *logindlg = new dialogs::LoginDialog(_widgetParent);
		logindlg->setWindowModality(Qt::WindowModal);
		logindlg->setAttribute(Qt::WA_DeleteOnClose);

		logindlg->setWindowTitle(_address.host());
		logindlg->setUsername(_address.userName(), true);

		if(_mustAuth)
			logindlg->setIntroText(tr("This server does not allow guest logins"));
		else
			logindlg->setIntroText(tr("Password needed to log in as \"%1\"").arg(_address.userName()));


		connect(logindlg, SIGNAL(rejected()), this, SLOT(cancelLogin()));
		connect(logindlg, SIGNAL(login(QString,QString)), this, SLOT(selectIdentity(QString,QString)));

		_state = WAIT_FOR_LOGIN_PASSWORD;
		logindlg->show();

	} else {
		sendIdentity();
	}
}

void LoginHandler::selectIdentity(const QString &password, const QString &username)
{
	_address.setUserName(username);
	_address.setPassword(password);
	sendIdentity();
}

void LoginHandler::sendIdentity()
{
	QString password = _address.password();
	QString ident = QString("IDENT \"%1\"").arg(_address.userName());

	if(!password.isEmpty())
		ident = ident + ";" + password;

	_state = EXPECT_IDENTIFIED;
	send(ident);
}

void LoginHandler::expectIdentified(const QString &msg)
{
	if(msg == "NEEDPASS") {
		// Looks like guest logins are not possible
		_needUserPassword = true;
		prepareToSendIdentity();
		return;
	}

	const QRegularExpression re("\\AIDENTIFIED (USER|GUEST) (-|[\\w,]+)\\z");
	auto m = re.match(msg);

	if(!m.hasMatch()) {
		qWarning() << "Login error. Expected IDENTIFIED, got:" << msg;
		failLogin(tr("Incompatible server"));
		return;
	}

	//bool isGuest = m.captured(1) == "GUEST";
	QStringList flags = m.captured(2).split(",", QString::SkipEmptyParts);

	if(_mode == HOST) {
		_state = EXPECT_SESSIONLIST_TO_HOST;

		// Query host password if needed
		if(_mode == HOST && _needHostPassword && !flags.contains("HOST")) {
			showPasswordDialog(tr("Password is needed to host a session"), tr("Enter hosting password"));
		}

	} else {
		// Show session selector if in multisession mode
		if(_multisession) {
			_selectorDialog = new dialogs::SelectSessionDialog(_sessions, _widgetParent);
			_selectorDialog->setWindowModality(Qt::WindowModal);
			_selectorDialog->setAttribute(Qt::WA_DeleteOnClose);

			connect(_selectorDialog, SIGNAL(selected(QString,bool)), this, SLOT(joinSelectedSession(QString,bool)));
			connect(_selectorDialog, SIGNAL(rejected()), this, SLOT(cancelLogin()));

			_selectorDialog->show();
		}

		_state = EXPECT_SESSIONLIST_TO_JOIN;
	}
}

void LoginHandler::expectSessionDescriptionHost(const QString &msg)
{
	Q_ASSERT(_mode == HOST);

	if(msg.startsWith("NOSESSION") || msg.startsWith("SESSION")) {
		// We don't care about existing sessions when hosting a new one,
		// but the reply means we can go ahead
		if(!_passwordDialog.isNull() && _hostPassword.isEmpty())
			_state = WAIT_FOR_HOST_PASSWORD; // password needed
		else
			sendHostCommand();

	} else if(msg.startsWith("TITLE")) {
		// title is not shown in this mode

	} else {
		qWarning() << "Expected session list, got" << msg;
		failLogin(tr("Incompatible server"));
		return;
	}
}

void LoginHandler::sendHostCommand()
{
	QString sessionId;
	if(!_hostSessionId.isEmpty())
		sessionId = _hostSessionId;
	else
		sessionId = "*";

	QString hostmsg = QString("HOST %1 %2 %3")
			.arg(sessionId)
			.arg(DRAWPILE_PROTO_MINOR_VERSION)
			.arg(_userid);

	if(!_hostPassword.isEmpty())
		hostmsg += ";" + _hostPassword;

	send(hostmsg);
	_state = EXPECT_LOGIN_OK;
}

void LoginHandler::expectSessionDescriptionJoin(const QString &msg)
{
	Q_ASSERT(_mode == JOIN);

	if(msg.startsWith("NOSESSION")) {
		// No sessions
		if(!_multisession) {
			failLogin(tr("Session does not exist yet!"));
			return;
		}

		int sep = msg.indexOf(' ');
		if(sep>0) {
			QString id = msg.mid(sep+1).trimmed();
			if(id.isEmpty())
				qWarning() << "invalid NOSESSION message:" << msg;
			else
				_sessions->removeSession(id);
		}
		return;

	} else if(msg.startsWith("TITLE ")) {
		// Server title update
		if(_selectorDialog)
			_selectorDialog->setServerTitle(msg.mid(6));
		return;
	}

	LoginSession session;

	// Expect session description in format:
	// SESSION [!]<id> <minorVersion> <FLAGS> <user-count> "founder" ;title
	const QRegularExpression re("\\ASESSION (!?[a-zA-Z0-9:-]{1,64}) (\\d+) (-|[\\w,]+) (\\d+) \"([^\"]+)\"\\s*;(.*)\\z");
	auto m = re.match(msg);

	if(!m.hasMatch()) {
		qWarning() << "Login error. Expected session description, got:" << msg;
		failLogin(tr("Incompatible server"));
		return;
	}

	session.id = m.captured(1);
	if(session.id.at(0) == '!') {
		session.id = session.id.mid(1);
		session.customId = true;
	}

	const int minorVersion = m.captured(2).toInt();
	if(minorVersion != DRAWPILE_PROTO_MINOR_VERSION) {
		qWarning() << "Session" << session.id << "minor version" << minorVersion << "mismatch.";
		session.incompatible = true;
	}

	const QStringList flags = m.captured(3).split(",");
	for(const QString &flag : flags) {
		if(flag=="-") {
			// No flags marker
		} else if(flag=="PASS") {
			session.needPassword = true;
		} else if(flag=="CLOSED") {
			session.closed = true;
		} else if(flag=="PERSIST") {
			session.persistent = true;
		} else if(flag=="ASLEEP") {
			session.asleep = true;
		} else {
			qWarning() << "Session" << session.id << "has unknown flag:" << flag;
			session.incompatible = true;
		}
	}

	session.userCount = m.captured(4).toInt();
	session.founder = m.captured(5);
	session.title = m.captured(6);

	if(session.id == _autoJoinId) {
		// A session ID was given as part of the URL
		joinSelectedSession(session.id, session.needPassword);

	} else if(_multisession) {
		// Multisesion mode: add session to list which is presented to the user
		_sessions->updateSession(session);

	} else {
		// Single session mode: automatically join the (only) session
		if(session.incompatible) {
			failLogin(tr("Session for a different Drawpile version in progress!"));
			return;
		}

		joinSelectedSession(session.id, session.needPassword);
	}
}

void LoginHandler::expectNoErrors(const QString &msg)
{
	// A "do nothing" handler while waiting for the user to enter a password
	if(msg.startsWith("SESSION") || msg.startsWith("NOSESSION") || msg.startsWith("TITLE"))
		return;

	qWarning() << "Unexpected login message:" << msg;
}

void LoginHandler::expectLoginOk(const QString &msg)
{
	if(msg.startsWith("SESSION") || msg.startsWith("NOSESSION") || msg.startsWith("TITLE")) {
		// We can still get session list updates here. They are safe to ignore.
		return;
	}

	if(msg.startsWith("OK ")) {
		const QRegularExpression re("\\AOK ([a-zA-Z0-9:-]{1,64}) (\\d+)\\z");
		auto m = re.match(msg);

		if(!m.hasMatch()) {
			qWarning() << "Login error. Expected OK <session> <id>, got:" << msg;
			failLogin(tr("Incompatible server"));
			return;
		}

		_loggedInSessionId = m.captured(1);
		_userid = m.captured(2).toInt();
		_server->loginSuccess();

		// If in host mode, send initial session settings
		if(_mode==HOST) {
			QStringList init;

			if(!_title.isEmpty())
				init << "title " + _title;

			if(!_sessionPassword.isEmpty())
				init << "password " + _sessionPassword;

			if(_maxusers>0)
				init << QString("maxusers %1").arg(_maxusers);

			if(!_allowdrawing)
				init <<  "lockdefault on";

			init << QStringLiteral("locklayerctrl ") + (_layerctrllock ? "on" : "off");

			if(_requestPersistent)
				init << "persistence on";

			if(_preserveChat)
				init << "preservechat on";

			if(!_announceUrl.isEmpty())
				init << "announce_at " + _announceUrl;

			for(const QString msg : init)
				_server->sendMessage(protocol::Chat::opCommand(0, msg));
		}

		delete _selectorDialog;
		delete _passwordDialog;
		delete _certDialog;

	} else {
		// Unexpected response
		qWarning() << "Login error. Unexpected response while waiting for OK:" << msg;
		failLogin(tr("Incompatible server"));
	}
}

void LoginHandler::joinSelectedSession(const QString &id, bool needPassword)
{
	_selectedId = id;
	if(needPassword) {
		showPasswordDialog(tr("Session is password protected"), tr("Enter session password"));
		_state = WAIT_FOR_JOIN_PASSWORD;

	} else {
		sendJoinCommand();
	}
}

void LoginHandler::sendJoinCommand()
{
	QString joinmsg = QString("JOIN %1").arg(_selectedId);

	if(!_joinPassword.isEmpty()) {
		joinmsg.append(';');
		joinmsg.append(_joinPassword);
	}

	send(joinmsg);
	_state = EXPECT_LOGIN_OK;
}

void LoginHandler::startTls()
{
	connect(_server->_socket, SIGNAL(encrypted()), this, SLOT(tlsStarted()));
	connect(_server->_socket, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(tlsError(QList<QSslError>)));

	_tls = false;
	_server->_socket->startClientEncryption();
}

void LoginHandler::tlsError(const QList<QSslError> &errors)
{
	QList<QSslError> ignore;
	QString errorstr;
	bool fail = false;

	bool isIp = QHostAddress().setAddress(_address.host());

	for(const QSslError &e : errors) {
		if(e.error() == QSslError::SelfSignedCertificate) {
			// Self signed certificates are acceptable.
			ignore << e;

		} else if(isIp && e.error() == QSslError::HostNameMismatch) {
			// Ignore CN mismatch when using an IP address rather than a hostname
			ignore << e;

		} else {
			fail = true;

			qWarning() << "SSL error:" << int(e.error()) << e.errorString();
			errorstr += e.errorString();
			errorstr += "\n";
		}
	}

	if(fail)
		failLogin(errorstr);
	else
		_server->_socket->ignoreSslErrors(ignore);
}

namespace {
void saveCert(const QFileInfo &file, const QSslCertificate &cert)
{
	QString filename = file.absoluteFilePath();

	QFile certOut(filename);

	if(certOut.open(QFile::WriteOnly))
		certOut.write(cert.toPem());
	else
		qWarning() << "Couldn't open" << filename << "for writing!";
}

}
void LoginHandler::tlsStarted()
{
	QSslCertificate cert = _server->hostCertificate();
	QString hostname = _address.host();

	// Check if this is a trusted certificate
	QFileInfo trustedCertFile = getCertFile(TRUSTED_HOSTS, hostname);
	if(trustedCertFile.exists()) {
		QList<QSslCertificate> trustedcerts = QSslCertificate::fromPath(trustedCertFile.absoluteFilePath());

		if(trustedcerts.isEmpty() || trustedcerts.at(0).isNull()) {
			failLogin(tr("Invalid SSL certificate for host %1").arg(hostname));

		} else if(trustedcerts.at(0) != cert) {
			failLogin(tr("Certificate of a trusted server has changed!"));

		} else {
			// Certificate matches explicitly trusted one, proceed with login
			_server->_securityLevel = Server::TRUSTED_HOST;
			tlsAccepted();
		}

		return;
	}

	// Check if we have seen this host certificate before
	QFileInfo certFile = getCertFile(KNOWN_HOSTS, hostname);
	if(certFile.exists()) {
		QList<QSslCertificate> knowncerts = QSslCertificate::fromPath(certFile.absoluteFilePath());

		if(knowncerts.isEmpty() || knowncerts.at(0).isNull()) {
			failLogin(tr("Invalid SSL certificate for host %1").arg(hostname));
			return;
		}

		if(knowncerts.at(0) != cert) {
			// Certificate mismatch!

			if(_selectorDialog)
				_selectorDialog->hide();

			_certDialog = new QMessageBox(_widgetParent);
			_certDialog->setWindowTitle(hostname);
			_certDialog->setWindowModality(Qt::WindowModal);
			_certDialog->setIcon(QMessageBox::Warning);
			_certDialog->setText(tr("The certificate of this server has changed!"));

			QAbstractButton *continueBtn = _certDialog->addButton(tr("Continue"), QMessageBox::AcceptRole);
			_certDialog->addButton(QMessageBox::Cancel);

			// WTF, accepted() and rejected() signals do not work correctly:
			// http://qt-project.org/forums/viewthread/21172
			// https://bugreports.qt-project.org/browse/QTBUG-23967
			connect(_certDialog.data(), &QMessageBox::finished, [this, cert, certFile, continueBtn]() {
				if(_certDialog->clickedButton() == continueBtn) {
					saveCert(certFile, cert);
					tlsAccepted();

				} else {
					cancelLogin();
				}
			});

			_certDialog->show();
			_server->_securityLevel = TcpServer::NEW_HOST;

			return;

		} else {
			_server->_securityLevel = TcpServer::KNOWN_HOST;
		}

	} else {
		// Host not encountered yet: rember the certificate for next time
		saveCert(certFile, cert);
		_server->_securityLevel = TcpServer::NEW_HOST;
	}

	// Certificate is acceptable
	tlsAccepted();
}

void LoginHandler::tlsAccepted()
{
	// STARTTLS is the very first command that must be sent, if sent at all
	// Next up is user authentication.
	prepareToSendIdentity();
}

void LoginHandler::cancelLogin()
{
	_state = ABORT_LOGIN;
	_server->loginFailure(tr("Cancelled"), "CANCELLED");
}

void LoginHandler::handleError(const QString &msg)
{
	const QString ecode = msg.mid(msg.indexOf(' ') + 1);
	qWarning() << "Login error:" << ecode;
	QString error;
	if(ecode == "NOSESSION")
		error = tr("Session not found!");
	else if(ecode == "BADPASS")
		error = tr("Incorrect password!");
	else if(ecode == "BADNAME")
		error = tr("Invalid username!");
	else if(ecode == "NAMEINUSE")
		error = tr("Username already taken!");
	else if(ecode == "CLOSED")
		error = tr("Session is closed!");
	else if(ecode == "BANNED")
		error = tr("This username has been banned!");
	else if(ecode == "SESSIONIDINUSE")
		error = tr("Session ID already in use!");
	else
		error = tr("Unknown error (%1)").arg(ecode);

	failLogin(error, ecode);
}

void LoginHandler::failLogin(const QString &message, const QString &errorcode)
{
	_state = ABORT_LOGIN;
	_server->loginFailure(message, errorcode);
}

void LoginHandler::send(const QString &message)
{
#ifdef DEBUG_LOGIN
	qDebug() << "login -->" << message;
#endif
	_server->sendMessage(protocol::MessagePtr(new protocol::Login(message)));
}

QString LoginHandler::sessionId() const {
	if(!_loggedInSessionId.isEmpty())
		return _loggedInSessionId;
	if(_mode == HOST)
		return _hostSessionId;
	else
		return _autoJoinId;
}

}
