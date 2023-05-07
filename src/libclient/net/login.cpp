// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/net/login.h"
#include "libclient/net/loginsessions.h"
#include "libclient/net/tcpserver.h"
#include "libclient/net/servercmd.h"
#include "libclient/contentfilter/contentfilter.h"

#include "libshared/net/protover.h"
#include "libshared/util/networkaccess.h"
#include "libshared/util/paths.h"

#include <QDebug>
#include <QStringList>
#include <QRegularExpression>
#include <QSslSocket>
#include <QFile>
#include <QHostAddress>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QImage>
#include <QBuffer>
#include <QRandomGenerator>
#include <QSettings>
#include <QUuid>

#ifndef NDEBUG
#define DEBUG_LOGIN
#endif

namespace {

enum CertLocation { KNOWN_HOSTS, TRUSTED_HOSTS };
QFileInfo getCertFile(CertLocation location, const QString &hostname)
{
	QString locationpath;
	if(location == KNOWN_HOSTS)
		locationpath = "known-hosts/";
	else
		locationpath = "trusted-hosts/";

	return QFileInfo(utils::paths::writablePath(locationpath, hostname + ".pem"));
}

}

namespace net {

LoginHandler::LoginHandler(Mode mode, const QUrl &url, QObject *parent)
	: QObject(parent)
	, m_mode(mode)
	, m_address(url)
	, m_state(EXPECT_HELLO)
	, m_multisession(false)
	, m_canPersist(false)
	, m_canReport(false)
	, m_needUserPassword(false)
	, m_supportsCustomAvatars(false)
	, m_supportsExtAuthAvatars(false)
	, m_isGuest(true)
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

bool LoginHandler::receiveMessage(const ServerReply &msg)
{
#ifdef DEBUG_LOGIN
	qInfo() << "login <--" << msg.reply;
#endif

	// Overall, the login process is:
	// 1. wait for server greeting
	// 2. Upgrade to secure connection (if available)
	// 3. Authenticate user (or do guest login)
	// 4. wait for session list
	// 5. wait for user to finish typing join password if needed
	// 6. send host/join command
	// 7. wait for OK

	if(msg.type == ServerReply::ReplyType::Error) {
		// The server disconnects us right after sending the error message
		handleError(msg.reply["code"].toString(), msg.message);
		return true;

	} else if(msg.type != ServerReply::ReplyType::Login && msg.type != ServerReply::ReplyType::Result) {
		qWarning() << "Login error: got reply type" << int(msg.type) << "when expected LOGIN, RESULT or ERROR";
		failLogin(tr("Invalid state"));

		return true;
	}

	switch(m_state) {
	case EXPECT_HELLO: expectHello(msg); break;
	case EXPECT_STARTTLS: expectStartTls(msg); break;
	case WAIT_FOR_LOGIN_PASSWORD:
	case WAIT_FOR_EXTAUTH:
		expectNothing(); break;
	case EXPECT_IDENTIFIED: expectIdentified(msg); break;
	case EXPECT_SESSIONLIST_TO_JOIN: expectSessionDescriptionJoin(msg); break;
	case EXPECT_SESSIONLIST_TO_HOST: expectSessionDescriptionHost(msg); break;
	case WAIT_FOR_JOIN_PASSWORD:
	case EXPECT_LOGIN_OK: return expectLoginOk(msg); break;
	case ABORT_LOGIN: /* ignore messages in this state */ break;
	}

	return true;
}

void LoginHandler::expectNothing()
{
	qWarning("Got login message while not expecting anything!");
	failLogin(tr("Incompatible server"));
}

void LoginHandler::expectHello(const ServerReply &msg)
{
	if(msg.type != ServerReply::ReplyType::Login) {
		qWarning() << "Login error. Greeting type is not LOGIN:" << int(msg.type);
		failLogin(tr("Incompatible server"));
		return;
	}

	// Server protocol version must match ours
	const int serverVersion = msg.reply["version"].toInt();
	if(serverVersion != protocol::ProtocolVersion::current().serverVersion()) {
		failLogin(tr("Server is for a different Drawpile version!"));
		return;
	}

	// Parse server capability flags
	const QJsonArray flags = msg.reply["flags"].toArray();

	m_mustAuth = false;
	m_needUserPassword = false;
	m_canPersist = false;
	m_canReport = false;

	bool startTls = false;

	for(const QJsonValue &flag : flags) {
		if(flag == "MULTI") {
			m_multisession = true;
		} else if(flag == "TLS") {
			startTls = true;
		} else if(flag == "SECURE") {
			// Changed in 2.1.9 (although in practice we've always done this): this flag is implied by TLS
		} else if(flag == "PERSIST") {
			m_canPersist = true;
		} else if(flag == "NOGUEST") {
			m_mustAuth = true;
		} else if(flag == "REPORT") {
			m_canReport = true;
		} else if(flag == "AVATAR") {
			m_supportsCustomAvatars = true;
		} else {
			qWarning() << "Unknown server capability:" << flag;
		}
	}

	// Start secure mode if possible
	if(startTls) {
		m_state = EXPECT_STARTTLS;
		send("startTls");

	} else {
		// If this is a trusted host, it should always be in secure mode
		if(getCertFile(TRUSTED_HOSTS, m_address.host()).exists()) {
			failLogin(tr("Secure mode not enabled on a trusted host!"));
			return;
		}

		prepareToSendIdentity();
	}
}

void LoginHandler::expectStartTls(const ServerReply &msg)
{
	if(msg.reply["startTls"].toBool()) {
		startTls();

	} else {
		qWarning() << "Login error. Expected startTls, got:" << msg.reply;
		failLogin(tr("Incompatible server"));
	}
}

void LoginHandler::sendSessionPassword(const QString &password)
{
	if(m_state == WAIT_FOR_JOIN_PASSWORD) {
		m_joinPassword = password;
		sendJoinCommand();

	} else {
		// shouldn't happen...
		qWarning("sendSessionPassword() in invalid state (%d)", m_state);
	}
}

void LoginHandler::prepareToSendIdentity()
{
	if(m_address.userName().isEmpty()) {
		emit usernameNeeded(m_supportsCustomAvatars);

	} else if(m_mustAuth || m_needUserPassword) {
		m_state = WAIT_FOR_LOGIN_PASSWORD;

		QString prompt;
		if(m_mustAuth)
			prompt = tr("This server does not allow guest logins");
		else
			prompt = tr("Password needed to log in as \"%1\"").arg(m_address.userName());

		emit loginNeeded(m_address.userName(), prompt);

	} else {
		sendIdentity();
	}
}

void LoginHandler::selectAvatar(const QImage &avatar)
{
	QBuffer a;
	avatar.save(&a, "PNG");
	m_avatar = a.buffer().toBase64();
}

void LoginHandler::selectIdentity(const QString &username, const QString &password)
{
	m_address.setUserName(username);
	m_address.setPassword(password);
	sendIdentity();
}

void LoginHandler::sendIdentity()
{
	QJsonArray args;
	QJsonObject kwargs;

	args << m_address.userName();

	if(!m_address.password().isEmpty())
		args << m_address.password();

	bool containsAvatar = !m_avatar.isEmpty();
	if(containsAvatar) {
		kwargs["avatar"] = QString::fromUtf8(m_avatar);
		// avatar needs only be sent once
		m_avatar = QByteArray();
	}

	kwargs["s"] = getSid();

	m_state = EXPECT_IDENTIFIED;
	send("ident", args, kwargs, containsAvatar);
}

void LoginHandler::requestExtAuth(const QString &username, const QString &password)
{
	// Construct request body
	QJsonObject o;
	o["username"] = username;
	o["password"] = password;
	if(!m_extAuthGroup.isEmpty())
		o["group"] = m_extAuthGroup;
	o["nonce"] = m_extAuthNonce;
	if(m_supportsExtAuthAvatars)
		o["avatar"] = true;

	// Send request
	QNetworkRequest req(m_extAuthUrl);
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	QNetworkReply *reply = networkaccess::getInstance()->post(req, QJsonDocument(o).toJson());
	connect(reply, &QNetworkReply::finished, this, [reply, this]() {
		reply->deleteLater();

		if(reply->error() != QNetworkReply::NoError) {
			failLogin(tr("Auth server error: %1").arg(reply->errorString()));
			return;
		}
		QJsonParseError error;
		const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &error);
		if(error.error != QJsonParseError::NoError) {
			failLogin(tr("Auth server error: %1").arg(error.errorString()));
			return;
		}

		const QJsonObject obj = doc.object();
		const QString status = obj["status"].toString();
		if(status == "auth") {
			m_state = EXPECT_IDENTIFIED;
			send("ident", { m_address.userName() }, {{"extauth", obj["token"]}});

			emit extAuthComplete(true);

		} else if(status == "badpass") {
			qWarning("Incorrect ext-auth password");

			emit extAuthComplete(false);

		} else if(status == "outgroup") {
			qWarning("Ext-auth error: group membership needed");
			failLogin(tr("Group membership needed"));

		} else {
			failLogin(tr("Unexpected ext-auth response: %1").arg(status));
		}
	});
}

void LoginHandler::expectIdentified(const ServerReply &msg)
{
	if(msg.reply["state"] == "needPassword") {
		// Looks like guest logins are not possible
		m_needUserPassword = true;
		prepareToSendIdentity();
		return;
	}

	if(msg.reply["state"] == "needExtAuth") {
		// External authentication needed for this username
		m_state = WAIT_FOR_EXTAUTH;
		m_extAuthUrl = msg.reply["extauthurl"].toString();
		m_supportsExtAuthAvatars = msg.reply["avatar"].toBool();

		if(!m_extAuthUrl.isValid()) {
			qWarning("Invalid ext-auth URL: %s", qPrintable(msg.reply["extauthurl"].toString()));
			failLogin(tr("Server misconfiguration: invalid ext-auth URL"));
			return;
		}
		if(m_extAuthUrl.scheme() != "http" && m_extAuthUrl.scheme() != "https") {
			qWarning("Unsupported ext-auth URL: %s", qPrintable(msg.reply["extauthurl"].toString()));
			failLogin(tr("Unsupported ext-auth URL scheme"));
			return;
		}
		m_extAuthGroup = msg.reply["group"].toString();
		m_extAuthNonce = msg.reply["nonce"].toString();

		emit extAuthNeeded(m_address.userName(), m_extAuthUrl);
		return;
	}

	if(msg.reply["state"] != "identOk") {
		qWarning() << "Expected identOk state, got" << msg.reply["state"];
		failLogin(tr("Invalid state"));
		return;
	}

	emit loginOk();

	m_isGuest = msg.reply["guest"].toBool();
	for(const QJsonValue f : msg.reply["flags"].toArray())
		m_userFlags << f.toString().toUpper();

	m_sessions->setModeratorMode(m_userFlags.contains("MOD"));

	if(m_mode != Mode::Join) {
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

void LoginHandler::expectSessionDescriptionHost(const ServerReply &msg)
{
	Q_ASSERT(m_mode != Mode::Join);

	if(msg.type == ServerReply::ReplyType::Login) {
		// We don't care about existing sessions when hosting a new one,
		// but the reply means we can go ahead
		sendHostCommand();

	} else {
		qWarning() << "Expected session list, got" << msg.reply;
		failLogin(tr("Incompatible server"));
		return;
	}
}

void LoginHandler::sendHostCommand()
{
	QJsonObject kwargs;

	if(!m_sessionAlias.isEmpty())
		kwargs["alias"] = m_sessionAlias;

	kwargs["protocol"] = protocol::ProtocolVersion::current().asString();
	kwargs["user_id"] = m_userid;
	if(!m_sessionPassword.isEmpty())
		kwargs["password"] = m_sessionPassword;

	send("host", {}, kwargs);
	m_state = EXPECT_LOGIN_OK;
}

void LoginHandler::expectSessionDescriptionJoin(const ServerReply &msg)
{
	Q_ASSERT(m_mode != Mode::HostRemote);

	if(msg.reply.contains("title")) {
		emit serverTitleChanged(msg.reply["title"].toString());
	}

	if(msg.reply.contains("sessions")) {
		const contentfilter::Level pclevel = contentfilter::level();

		for(const auto jsv : msg.reply["sessions"].toArray()) {
			const QJsonObject js = jsv.toObject();

			m_protocolVersion = protocol::ProtocolVersion::fromString(js["protocol"].toString());

			QString incompatibleSeries;
			if(!m_protocolVersion.isCompatible()) {
				if(m_protocolVersion.isFuture()) {
					incompatibleSeries = tr("New version");
				} else {
					incompatibleSeries = m_protocolVersion.versionName();
				}

				if(incompatibleSeries.isEmpty()) {
					incompatibleSeries = tr("Unknown version");
				}
			}

			const LoginSession session {
				js["id"].toString(),
				js["alias"].toString(),
				js["title"].toString(),
				js["founder"].toString(),
				incompatibleSeries,
				m_protocolVersion.isPastCompatible(),
				js["userCount"].toInt(),
				js["hasPassword"].toBool(),
				js["persistent"].toBool(),
				js["closed"].toBool() || (js["authOnly"].toBool() && m_isGuest),
				js["nsfm"].toBool()
			};

			m_sessions->updateSession(session);

			if(!m_autoJoinId.isEmpty() && (session.id == m_autoJoinId || session.alias == m_autoJoinId)) {
				// A session ID was given as part of the URL

				auto canJoin = session.incompatibleSeries.isEmpty();
				if (canJoin && pclevel >= contentfilter::Level::NoJoin) {
					canJoin = (contentfilter::useAdvisoryTag() ? !session.nsfm : true)
						&& !contentfilter::isNsfmTitle(session.title);
				}

				if(canJoin)
					joinSelectedSession(m_autoJoinId, session.needPassword);
				else
					m_autoJoinId = QString();
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

		LoginSession session = m_sessions->getFirstSession();

		if(session.id.isEmpty()) {
			failLogin(tr("Session not yet started!"));
			return;
		} else if(contentfilter::level() >= contentfilter::Level::NoJoin) {
			const auto blocked = (contentfilter::useAdvisoryTag() ? session.nsfm : false)
				|| contentfilter::isNsfmTitle(session.title);

			if (blocked) {
				failLogin(tr("Blocked by content filter"));
				return;
			}
		} else if(!session.incompatibleSeries.isEmpty()) {
			failLogin(tr("Session for a different Drawpile version (%1) in progress!").arg(session.incompatibleSeries));
			return;
		}

		joinSelectedSession(session.id, session.needPassword);
	}
}

void LoginHandler::expectNoErrors(const ServerReply &msg)
{
	// A "do nothing" handler while waiting for the user to enter a password
	if(msg.type == ServerReply::ReplyType::Login)
		return;

	qWarning() << "Unexpected login message:" << msg.reply;
}

bool LoginHandler::expectLoginOk(const ServerReply &msg)
{
	if(msg.type == ServerReply::ReplyType::Login) {
		// We can still get session list updates here. They are safe to ignore.
		return true;
	}

	if(msg.reply["state"] == "join" || msg.reply["state"] == "host") {
		m_address.setPath("/" + msg.reply["join"].toObject()["id"].toString());
		const int userid = msg.reply["join"].toObject()["user"].toInt();

		if(userid < 1 || userid > 254) {
			qWarning() << "Login error. User ID" << userid << "out of supported range.";
			failLogin(tr("Incompatible server"));
			return true;
		}

		m_userid = uint8_t(userid);

		const QJsonArray sessionFlags = msg.reply["join"].toObject()["flags"].toArray();
		for(const QJsonValue &val : sessionFlags) {
			if(val.isString())
				m_sessionFlags << val.toString();
		}

		m_server->loginSuccess();

		// If in host mode, send initial session settings
		if(m_mode != Mode::Join) {
			QJsonObject kwargs;

			if(!m_title.isEmpty())
				kwargs["title"] = m_title;

			if(contentfilter::isNsfmTitle(m_title))
				kwargs["nsfm"] = true;

			send("sessionconf", {}, kwargs);

			if(!m_announceUrl.isEmpty())
				m_server->sendMessage(ServerCommand::makeAnnounce(m_announceUrl, m_announcePrivate));

			// Upload initial session content
			if(m_mode == Mode::HostRemote) {
				m_server->sendMessages(m_initialState.count(), m_initialState.constData());
				send("init-complete");
			}
		}

		// Login phase over: any remaining messages belong to the session
		return false;

	} else {
		// Unexpected response
		qWarning() << "Login error. Unexpected response while waiting for OK:" << msg.reply;
		failLogin(tr("Incompatible server"));
	}

	return true;
}

void LoginHandler::joinSelectedSession(const QString &id, bool needPassword)
{
	Q_ASSERT(!id.isEmpty());
	m_selectedId = id;
	if(needPassword && !m_sessions->isModeratorMode()) {
		emit sessionPasswordNeeded();
		m_state = WAIT_FOR_JOIN_PASSWORD;

	} else {
		sendJoinCommand();
	}
}

void LoginHandler::sendJoinCommand()
{
	QJsonObject kwargs;
	if(!m_joinPassword.isEmpty()) {
		kwargs["password"] = m_joinPassword;
	}

	send("join", { m_selectedId }, kwargs);
	m_state = EXPECT_LOGIN_OK;
}

void LoginHandler::reportSession(const QString &id, const QString &reason)
{
	send("report", {}, {
		{"session", id},
		{"reason", reason}
	});
}

void LoginHandler::startTls()
{
	connect(m_server->m_socket, &QSslSocket::encrypted, this, &LoginHandler::tlsStarted);
	connect(m_server->m_socket, QOverload<const QList<QSslError>&>::of(&QSslSocket::sslErrors), this, &LoginHandler::tlsError);

	m_server->m_socket->startClientEncryption();
}

void LoginHandler::tlsError(const QList<QSslError> &errors)
{
	QList<QSslError> ignore;
	QString errorstr;
	bool fail = false;

	// TODO this was optimized for self signed certificates back end Let's Encrypt
	// didn't exist. This should be fixed to better support actual CAs.
	bool isIp = QHostAddress().setAddress(m_address.host());

	for(const QSslError &e : errors) {
		if(e.error() == QSslError::SelfSignedCertificate) {
			// Self signed certificates are acceptable.
			ignore << e;

		} else if(isIp && e.error() == QSslError::HostNameMismatch) {
			// Ignore CN mismatch when using an IP address rather than a hostname
			ignore << e;

#ifdef Q_OS_MACOS
		} else if(e.error() == QSslError::CertificateUntrusted) {
			// Ignore "The root CA certificate is not trusted for this purpose" error that
			// started happening on OSX. We still check that the certificate matches the one
			// we have saved. (Most server certs are expected to be self-signed)
			ignore << e;
#endif
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
		m_server->m_socket->ignoreSslErrors(ignore);
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
			m_server->m_securityLevel = Server::TRUSTED_HOST;
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
			m_server->m_securityLevel = TcpServer::NEW_HOST;
			return;

		} else {
			m_server->m_securityLevel = TcpServer::KNOWN_HOST;
		}

	} else {
		// Host not encountered yet: rember the certificate for next time
		saveCert(m_certFile, cert);
		m_server->m_securityLevel = TcpServer::NEW_HOST;
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
	else if(code == "badPassword") {
		error = tr("Incorrect password!");
		emit badLoginPassword();
	} else if(code == "badUsername")
		error = tr("Invalid username!");
	else if(code == "bannedName")
		error = tr("This username has been locked");
	else if(code == "nameInUse")
		error = tr("Username already taken!");
	else if(code == "closed")
		error = m_mode == Mode::Join ? tr("Session is closed!") : tr("Server is full!");
	else if(code == "unauthorizedHost")
		error = tr("Hosting not authorized");
	else if(code == "banned")
		error = tr("You have been banned from this session!");
	else if(code == "idInUse")
		error = tr("Session alias is reserved!");
	else
		error = msg;

	failLogin(error, code);
}

void LoginHandler::failLogin(const QString &message, const QString &errorcode)
{
	m_state = ABORT_LOGIN;
	m_server->loginFailure(message, errorcode);
}

void LoginHandler::send(
	const QString &cmd, const QJsonArray &args, const QJsonObject &kwargs,
	bool containsAvatar)
{
	ServerCommand sc { cmd, args, kwargs };
	drawdance::Message msg = sc.toMessage();
	if(msg.isNull() && containsAvatar) {
		qWarning("Removing avatar from server command and trying again");
		sc.kwargs.remove("avatar");
		msg = sc.toMessage();
	}

	if(msg.isNull()) {
		failLogin(tr("Client failed to serialize command"));
	} else {
#ifdef DEBUG_LOGIN
		qInfo() << "login -->" << cmd << args << kwargs;
#endif
		m_server->sendMessage(msg);
	}
}

bool LoginHandler::hasUserFlag(const QString &flag) const
{
	return m_userFlags.contains(flag.toUpper());
}

QString LoginHandler::getSid()
{
	static QString org{"4428c43c3a5c4d3fa20002c21d2b2c2e"};
	static QString app{"56e06fe173a345dd983fe63671188093"};
	static QString key1{"5dd7038779f243b68001dab548ae59ab"};
	static QString key2{"56e06fe173a345dd983fe63671188093"};

	QSettings cfg1{
		QSettings::NativeFormat, QSettings::UserScope,
		QStringLiteral("drawpile"), QStringLiteral("sid")};
	QSettings cfg2{
		QSettings::NativeFormat, QSettings::UserScope, org, app};

	bool haveSid1 = cfg1.contains(key1);
	bool haveSid2 = cfg2.contains(key2);
	if(haveSid1 && haveSid2) {
		QString sid1 = cfg1.value(key1).toString();
		QString sid2 = cfg2.value(key2).toString();
		return sid1 == sid2 ? sid1 : generateTamperSid();
	} else if(haveSid1 && !haveSid2) {
		QString sid = cfg1.value(key1).toString();
		cfg2.setValue(key2, sid);
		return sid;
	} else if(!haveSid1 && haveSid2) {
		QString sid = cfg2.value(key2).toString();
		cfg1.setValue(key1, sid);
		return sid;
	} else {
		QString sid = generateSid();
		cfg1.setValue(key1, sid);
		cfg2.setValue(key2, sid);
		return sid;
	}
}

QString LoginHandler::generateTamperSid()
{
	QString sid = generateSid();
	int pos = QRandomGenerator::global()->bounded(sid.length() - 1);
	sid.replace(pos, 1, 'O');
	return sid;
}

QString LoginHandler::generateSid()
{
	return QUuid::createUuid().toString(QUuid::Id128);
}

}
