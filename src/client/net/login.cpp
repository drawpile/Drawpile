/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2016 Calle Laakkonen

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

#include "net/login.h"
#include "net/loginsessions.h"
#include "net/tcpserver.h"

#include "../shared/net/control.h"
#include "../shared/net/meta.h"
#include "../shared/net/meta2.h"

#include <QDebug>
#include <QStringList>
#include <QRegularExpression>
#include <QSslSocket>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QHostAddress>

#define DEBUG_LOGIN

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

LoginHandler::LoginHandler(Mode mode, const QUrl &url, QObject *parent)
	: QObject(parent),
	  m_mode(mode),
	  m_address(url),
	  m_maxusers(0),
	  m_allowdrawing(true),
	  m_layerctrllock(true),
	  m_state(EXPECT_HELLO),
	  m_multisession(false),
	  m_tls(false)
{
	m_sessions = new LoginSessionModel(this);

	// Automatically join a session if the ID is included in the URL
	QString path = m_address.path();
	if(path.length()>1) {
		QRegularExpression idre("\\A/([a-zA-Z0-9:-]{1,64})/?\\z");
		auto m = idre.match(path);
		if(m.hasMatch())
			m_autoJoinId = m.captured(1);
	}
}

void LoginHandler::serverDisconnected()
{
}

void LoginHandler::receiveMessage(protocol::MessagePtr message)
{
	if(message->type() == protocol::MSG_DISCONNECT) {
		// server reports login errors with MSG_COMMAND, so there is nothing
		// of more interest here.
		return;
	}

	if(message->type() != protocol::MSG_COMMAND) {
		qWarning() << "Login error: got message type" << message->type() << "when expected type 0";
		failLogin(tr("Invalid state"));
		return;
	}

	const protocol::ServerReply msg = message.cast<protocol::Command>().reply();

#ifdef DEBUG_LOGIN
	qDebug() << "login <--" << msg.reply;
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

	if(msg.type == protocol::ServerReply::ERROR) {
		// The server disconnects us right after sending the error message
		handleError(msg.reply["code"].toString(), msg.message);
		return;

	} else if(msg.type != protocol::ServerReply::LOGIN && msg.type != protocol::ServerReply::RESULT) {
		qWarning() << "Login error: got reply type" << msg.type << "when expected LOGIN, RESULT or ERROR";
		failLogin(tr("Invalid state"));
	}

	switch(m_state) {
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

void LoginHandler::expectNothing(const protocol::ServerReply &msg)
{
	Q_UNUSED(msg);
	qWarning("Got login message while not expecting anything!");
	failLogin(tr("Incompatible server"));
}

void LoginHandler::expectHello(const protocol::ServerReply &msg)
{
	if(msg.type != protocol::ServerReply::LOGIN) {
		qWarning() << "Login error. Greeting type is not LOGIN:" << msg.type;
		failLogin(tr("Incompatible server"));
		return;
	}

	// Server protocol version must match ours
	const int serverVersion = msg.reply["version"].toInt();
	if(serverVersion != DRAWPILE_PROTO_SERVER_VERSION) {
		failLogin(tr("Server is for a different Drawpile version!"));
		return;
	}

	// Parse server capability flags
	const QJsonArray flags = msg.reply["flags"].toArray();

	bool mustSecure = false;
	m_needHostPassword = false;
	m_canAuth = false;
	m_mustAuth = false;
	m_needUserPassword = false;

	for(const QJsonValue &flag : flags) {
		if(flag == "MULTI") {
			m_multisession = true;
		} else if(flag == "HOSTP") {
			m_needHostPassword = true;
		} else if(flag == "TLS") {
			m_tls = true;
		} else if(flag == "SECURE") {
			mustSecure = true;
		} else if(flag == "PERSIST") {
			// TODO indicate persistent session support
		} else if(flag == "IDENT") {
			m_canAuth = true;
		} else if(flag == "NOGUEST") {
			m_mustAuth = true;
		} else {
			qWarning() << "Unknown server capability:" << flag;
		}
	}

	// Start secure mode if possible
	if(QSslSocket::supportsSsl() && m_tls) {
		m_state = EXPECT_STARTTLS;

		protocol::ServerCommand cmd;
		cmd.cmd = "startTls";
		send(cmd);

	} else {
		// If this is a trusted host, it should always be in secure mode
		if(QSslSocket::supportsSsl() && getCertFile(TRUSTED_HOSTS, m_address.host()).exists()) {
			failLogin(tr("Secure mode not enabled on a trusted host!"));
			return;
		}

		if(mustSecure) {
			failLogin(tr("This is a secure secure server, but secure connection support is not available!"));
			return;
		}

		m_tls = false;
		prepareToSendIdentity();
	}
}

void LoginHandler::expectStartTls(const protocol::ServerReply &msg)
{
	Q_ASSERT(m_tls);
	if(msg.reply["startTls"].toBool()) {
		startTls();

	} else {
		qWarning() << "Login error. Expected startTls, got:" << msg.reply;
		failLogin(tr("Incompatible server"));
	}
}

void LoginHandler::gotPassword(const QString &password)
{
	switch(m_state) {
	case WAIT_FOR_HOST_PASSWORD:
		m_hostPassword = password;
		sendHostCommand();
		break;
	case WAIT_FOR_JOIN_PASSWORD:
		m_joinPassword = password;
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
	if(m_mustAuth || m_needUserPassword) {
		m_state = WAIT_FOR_LOGIN_PASSWORD;

		QString prompt;
		if(m_mustAuth)
			prompt = tr("This server does not allow guest logins");
		else
			prompt = tr("Password needed to log in as \"%1\"").arg(m_address.userName());

		emit loginNeeded(prompt);

	} else {
		sendIdentity();
	}
}

void LoginHandler::selectIdentity(const QString &username, const QString &password)
{
	m_address.setUserName(username);
	m_address.setPassword(password);
	sendIdentity();
}

void LoginHandler::sendIdentity()
{
	protocol::ServerCommand cmd;
	cmd.cmd = "ident";
	cmd.args.append(m_address.userName());

	if(!m_address.password().isEmpty())
		cmd.args.append(m_address.password());

	m_state = EXPECT_IDENTIFIED;
	send(cmd);
}

void LoginHandler::expectIdentified(const protocol::ServerReply &msg)
{
	if(msg.reply["state"] == "needPassword") {
		// Looks like guest logins are not possible
		m_needUserPassword = true;
		prepareToSendIdentity();
		return;
	}

	if(msg.reply["state"] != "identOk") {
		qWarning() << "Expected identOk state, got" << msg.reply["state"];
		failLogin(tr("Invalid state"));
		return;
	}

	//bool isGuest = msg.reply["guest"].toBool();
	QJsonArray flags = msg.reply["flags"].toArray();

	if(m_mode == HOST) {
		m_state = EXPECT_SESSIONLIST_TO_HOST;

	} else {
		// Show session selector if in multisession mode
		// In single-session mode we can just automatically join
		// the first session we see.
		if(m_multisession)
			emit sessionChoiceNeeded(m_sessions);

		m_state = EXPECT_SESSIONLIST_TO_JOIN;
	}
}

void LoginHandler::expectSessionDescriptionHost(const protocol::ServerReply &msg)
{
	Q_ASSERT(m_mode == HOST);

	if(msg.type == protocol::ServerReply::LOGIN) {
		// We don't care about existing sessions when hosting a new one,
		// but the reply means we can go ahead
		if(m_needHostPassword) {
			m_state = WAIT_FOR_HOST_PASSWORD;
			emit passwordNeeded(tr("Enter hosting password"));

		} else {
			sendHostCommand();
		}

	} else {
		qWarning() << "Expected session list, got" << msg.reply;
		failLogin(tr("Incompatible server"));
		return;
	}
}

void LoginHandler::sendHostCommand()
{
	protocol::ServerCommand cmd;
	cmd.cmd = "host";

	if(!m_hostSessionId.isEmpty())
		cmd.kwargs["id"] = m_hostSessionId;

	cmd.kwargs["protocol"] = DRAWPILE_PROTO_STR;
	cmd.kwargs["user_id"] = m_userid;
	if(!m_hostPassword.isEmpty())
		cmd.kwargs["host_password"] = m_hostPassword;
	// TODO session password

	send(cmd);
	m_state = EXPECT_LOGIN_OK;
}

void LoginHandler::expectSessionDescriptionJoin(const protocol::ServerReply &msg)
{
	Q_ASSERT(m_mode == JOIN);

	if(msg.reply.contains("title")) {
		emit serverTitleChanged(msg.reply["title"].toString());
	}

	if(msg.reply.contains("sessions")) {
		for(const QJsonValue &jsv : msg.reply["sessions"].toArray()) {
			QJsonObject js = jsv.toObject();
			LoginSession session;

			session.id = js["id"].toString();
			if(session.id.startsWith('!')) {
				session.id = session.id.mid(1);
				session.customId = true;
			}

			const QString protoVer = js["protocol"].toString();
			session.incompatible = protoVer != DRAWPILE_PROTO_STR;
			qDebug() << protoVer << " != " << DRAWPILE_PROTO_STR;
			session.needPassword = js["password"].toBool();
			session.closed = js["closed"].toBool();
			session.asleep = js["asleep"].toBool();
			session.persistent = js["persistent"].toBool();
			session.userCount = js["users"].toInt();
			session.founder = js["founder"].toString();
			session.title = js["title"].toString();

			m_sessions->updateSession(session);

			if(session.id == m_autoJoinId) {
				// A session ID was given as part of the URL
				joinSelectedSession(session.id, session.needPassword);
			}
		}
	}

	if(msg.reply.contains("remove")) {
		for(const QJsonValue j : msg.reply["remove"].toArray()) {
			m_sessions->removeSession(j.toString());
		}
	}

	if(!m_multisession) {
		// Single session mode: automatically join the (only) session

		if(m_sessions->rowCount()==0) {
			failLogin(tr("Session not yet started!"));

		} else {
			LoginSession session = m_sessions->sessionAt(0);

			if(session.incompatible) {
				failLogin(tr("Session for a different Drawpile version in progress!"));

			} else {
				joinSelectedSession(session.id, session.needPassword);
			}
		}
	}
}

void LoginHandler::expectNoErrors(const protocol::ServerReply &msg)
{
	// A "do nothing" handler while waiting for the user to enter a password
	if(msg.type == protocol::ServerReply::LOGIN)
		return;

	qWarning() << "Unexpected login message:" << msg.reply;
}

void LoginHandler::expectLoginOk(const protocol::ServerReply &msg)
{
	if(msg.type == protocol::ServerReply::LOGIN) {
		// We can still get session list updates here. They are safe to ignore.
		return;
	}

	if(msg.reply["state"] == "join" || msg.reply["state"] == "host") {
		m_loggedInSessionId = msg.reply["join"].toObject()["id"].toString();
		m_userid = msg.reply["join"].toObject()["user"].toInt();
		m_server->loginSuccess();

		// If in host mode, send initial session settings
		if(m_mode==HOST) {
			protocol::ServerCommand conf;
			conf.cmd = "sessionconf";

			if(!m_title.isEmpty())
				conf.kwargs["title"] = m_title;

			if(!m_sessionPassword.isEmpty())
				conf.kwargs["password"] = m_sessionPassword;

			if(m_maxusers>0)
				conf.kwargs["max-users"] = m_maxusers;

			if(m_requestPersistent)
				conf.kwargs["persistent"] = true;

			if(m_preserveChat)
				conf.kwargs["preserve-chat"] = true;

			m_server->sendMessage(protocol::MessagePtr(new protocol::Command(userId(), conf)));

			uint16_t lockflags = 0;

			if(!m_allowdrawing)
				lockflags |= protocol::SessionACL::LOCK_DEFAULT;

			if(m_layerctrllock)
				lockflags |= protocol::SessionACL::LOCK_LAYERCTRL;

			if(lockflags)
				m_server->sendMessage(protocol::MessagePtr(new protocol::SessionACL(userId(), lockflags)));

			if(!m_announceUrl.isEmpty()) {
				protocol::ServerCommand cmd;
				cmd.cmd = "announce-session";
				cmd.args = QJsonArray() << m_announceUrl;
				m_server->sendMessage(protocol::MessagePtr(new protocol::Command(userId(), cmd)));
			}

			m_server->sendSnapshotMessages(m_initialState);
		}

	} else {
		// Unexpected response
		qWarning() << "Login error. Unexpected response while waiting for OK:" << msg.reply;
		failLogin(tr("Incompatible server"));
	}
}

void LoginHandler::joinSelectedSession(const QString &id, bool needPassword)
{
	m_selectedId = id;
	if(needPassword) {
		emit passwordNeeded(tr("Enter session password"));
		m_state = WAIT_FOR_JOIN_PASSWORD;

	} else {
		sendJoinCommand();
	}
}

void LoginHandler::sendJoinCommand()
{
	protocol::ServerCommand cmd;
	cmd.cmd = "join";
	cmd.args.append(m_selectedId);

	if(!m_joinPassword.isEmpty()) {
		cmd.kwargs["password"] = m_joinPassword;
	}

	send(cmd);
	m_state = EXPECT_LOGIN_OK;
}

void LoginHandler::startTls()
{
	connect(m_server->_socket, SIGNAL(encrypted()), this, SLOT(tlsStarted()));
	connect(m_server->_socket, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(tlsError(QList<QSslError>)));

	m_tls = false;
	m_server->_socket->startClientEncryption();
}

void LoginHandler::tlsError(const QList<QSslError> &errors)
{
	QList<QSslError> ignore;
	QString errorstr;
	bool fail = false;

	bool isIp = QHostAddress().setAddress(m_address.host());

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
		m_server->_socket->ignoreSslErrors(ignore);
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
	const QSslCertificate cert = m_server->hostCertificate();
	const QString hostname = m_address.host();

	// Check if this is a trusted certificate
	m_certFile = getCertFile(TRUSTED_HOSTS, hostname);

	if(m_certFile.exists()) {
		QList<QSslCertificate> trustedcerts = QSslCertificate::fromPath(m_certFile.absoluteFilePath());

		if(trustedcerts.isEmpty() || trustedcerts.at(0).isNull()) {
			failLogin(tr("Invalid SSL certificate for host %1").arg(hostname));

		} else if(trustedcerts.at(0) != cert) {
			failLogin(tr("Certificate of a trusted server has changed!"));

		} else {
			// Certificate matches explicitly trusted one, proceed with login
			m_server->_securityLevel = Server::TRUSTED_HOST;
			continueTls();
		}

		return;
	}

	// Okay, not a trusted certificate, but check if we've seen it before
	m_certFile = getCertFile(KNOWN_HOSTS, hostname);
	if(m_certFile.exists()) {
		QList<QSslCertificate> knowncerts = QSslCertificate::fromPath(m_certFile.absoluteFilePath());

		if(knowncerts.isEmpty() || knowncerts.at(0).isNull()) {
			failLogin(tr("Invalid SSL certificate for host %1").arg(hostname));
			return;
		}

		if(knowncerts.at(0) != cert) {
			// Certificate mismatch!
			emit certificateCheckNeeded(cert, knowncerts.at(0));
			m_server->_securityLevel = TcpServer::NEW_HOST;
			return;

		} else {
			m_server->_securityLevel = TcpServer::KNOWN_HOST;
		}

	} else {
		// Host not encountered yet: rember the certificate for next time
		saveCert(m_certFile, cert);
		m_server->_securityLevel = TcpServer::NEW_HOST;
	}

	// Certificate is acceptable
	continueTls();
}

void LoginHandler::acceptServerCertificate()
{
	// User accepted the mismatching certificate: save it for the future
	saveCert(m_certFile, m_server->hostCertificate());
	continueTls();
}

void LoginHandler::continueTls()
{
	// STARTTLS is the very first command that must be sent, if sent at all
	// Next up is user authentication.
	prepareToSendIdentity();
}

void LoginHandler::cancelLogin()
{
	m_state = ABORT_LOGIN;
	m_server->loginFailure(tr("Cancelled"), "CANCELLED");
}

void LoginHandler::handleError(const QString &code, const QString &msg)
{
	qWarning() << "Login error:" << code << msg;

	QString error;
	if(code == "notFound")
		error = tr("Session not found!");
	else if(code == "badPassword")
		error = tr("Incorrect password!");
	else if(code == "badUsername")
		error = tr("Invalid username!");
	else if(code == "nameInUse")
		error = tr("Username already taken!");
	else if(code == "closed")
		error = tr("Session is closed!");
	else if(code == "banned")
		error = tr("This username has been banned!");
	else if(code == "sessionIdInUse")
		error = tr("Session ID already in use!");
	else
		error = msg;

	failLogin(error, code);
}

void LoginHandler::failLogin(const QString &message, const QString &errorcode)
{
	m_state = ABORT_LOGIN;
	m_server->loginFailure(message, errorcode);
}

void LoginHandler::send(const protocol::ServerCommand &cmd)
{
#ifdef DEBUG_LOGIN
	qDebug() << "login -->" << cmd.toJson();
#endif
	m_server->sendMessage(protocol::MessagePtr(new protocol::Command(0, cmd)));
}

QString LoginHandler::sessionId() const {
	if(!m_loggedInSessionId.isEmpty())
		return m_loggedInSessionId;
	if(m_mode == HOST)
		return m_hostSessionId;
	else
		return m_autoJoinId;
}

}
