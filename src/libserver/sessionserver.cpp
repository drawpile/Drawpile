// SPDX-License-Identifier: GPL-3.0-or-later
#include "libserver/sessionserver.h"
#include "libserver/announcements.h"
#include "libserver/filedhistory.h"
#include "libserver/inmemoryhistory.h"
#include "libserver/loginhandler.h"
#include "libserver/serverconfig.h"
#include "libserver/serverlog.h"
#include "libserver/templateloader.h"
#include "libserver/thinserverclient.h"
#include "libserver/thinsession.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>

namespace server {

SessionServer::SessionServer(ServerConfig *config, QObject *parent)
	: QObject(parent)
	, m_config(config)
{
	m_announcements = new sessionlisting::Announcements(config, this);

	QTimer *cleanupTimer = new QTimer(this);
	connect(
		cleanupTimer, &QTimer::timeout, this, &SessionServer::cleanupSessions);
	cleanupTimer->setInterval(15 * 1000);
	cleanupTimer->start(cleanupTimer->interval());
}

void SessionServer::setSessionDir(const QDir &dir)
{
	if(dir.isReadable()) {
		m_sessiondir = dir;
		m_useFiledSessions = true;
		loadNewSessions();
	} else {
		qWarning("%s is not readable", qUtf8Printable(dir.absolutePath()));
	}
}

void SessionServer::loadNewSessions()
{
	if(!m_useFiledSessions) {
		return;
	}

	QFileInfoList sessionFiles = m_sessiondir.entryInfoList(
		{QStringLiteral("*.session")},
		QDir::Files | QDir::Writable | QDir::Readable);
	for(const QFileInfo &f : sessionFiles) {
		if(getSessionById(f.baseName(), false)) {
			continue;
		}

		FiledHistory *fh = FiledHistory::load(f.absoluteFilePath(), this);
		if(fh) {
			Session *session =
				new ThinSession(fh, m_config, m_announcements, this);
			initSession(session);
			session->log(
				Log()
					.about(Log::Level::Debug, Log::Topic::Status)
					.message(QStringLiteral("Loaded from file.")));
		}
	}
}

bool SessionServer::shouldArchive() const
{
	return m_config->getConfigBool(config::ArchiveMode);
}

QJsonArray SessionServer::sessionDescriptions(bool includeUnlisted) const
{
	QJsonArray descs;
	QSet<QString> aliases;

	for(const Session *s : m_sessions) {
		if(includeUnlisted ||
		   !s->history()->hasFlag(SessionHistory::Unlisted)) {
			descs.append(s->getDescription());
		}
		if(!s->idAlias().isEmpty()) {
			aliases.insert(s->idAlias());
		}
	}

	const TemplateLoader *loader = templateLoader();
	bool supportsWebSockets = m_config->internalConfig().webSocket;
	if(loader) {
		// Add session templates to list, if not shadowed by live sessions
		QVector<QJsonObject> templates =
			loader->templateDescriptions(includeUnlisted);
		for(QJsonObject &o : templates) {
			QString alias = o.value(QStringLiteral("alias")).toString();
			if(!aliases.contains(alias)) {
				aliases.insert(alias);
				if(!supportsWebSockets) {
					o.remove(QStringLiteral("allowWeb"));
				}
				descs.append(o);
			}
		}
	}

	return descs;
}

SessionHistory *SessionServer::initHistory(
	const QString &id, const QString alias,
	const protocol::ProtocolVersion &protocolVersion, const QString &founder)
{
	if(m_useFiledSessions) {
		FiledHistory *fh = FiledHistory::startNew(
			m_sessiondir, id, alias, protocolVersion, founder, this);
		return fh;
	} else {
		return new InMemoryHistory(id, alias, protocolVersion, founder);
	}
}

std::tuple<Session *, QString> SessionServer::createSession(
	const QString &id, const QString &idAlias,
	const protocol::ProtocolVersion &protocolVersion, const QString &founder)
{
	Q_ASSERT(!id.isNull());

	if(m_sessions.size() >= m_config->getConfigInt(config::SessionCountLimit)) {
		return std::tuple<Session *, QString>{
			nullptr, QStringLiteral("closed")};
	}

	if(getSessionById(id, false) ||
	   (!idAlias.isEmpty() && getSessionById(idAlias, false))) {
		return std::tuple<Session *, QString>{
			nullptr, QStringLiteral("idInUse")};
	}

	if(protocolVersion.serverVersion() !=
	   protocol::ProtocolVersion::current().serverVersion()) {
		return std::tuple<Session *, QString>{
			nullptr, QStringLiteral("badProtocol")};
	}

	SessionHistory *history =
		initHistory(id, idAlias, protocolVersion, founder);
	int userLimit =
		qBound(2, m_config->getConfigInt(config::SessionUserLimit), 254);
	if(history->maxUsers() != userLimit) {
		history->setMaxUsers(userLimit);
	}

	Session *session =
		new ThinSession(history, m_config, m_announcements, this);

	initSession(session);

	QString aka = idAlias.isEmpty() ? QString()
									: QStringLiteral(" (AKA %1)").arg(idAlias);

	session->log(
		Log()
			.about(Log::Level::Info, Log::Topic::Status)
			.message(
				QStringLiteral("Session %1 created by %2").arg(aka, founder)));

	return std::make_tuple(session, QString());
}

Session *SessionServer::createFromTemplate(const QString &idAlias)
{
	Q_ASSERT(templateLoader());

	QJsonObject desc = templateLoader()->templateDescription(idAlias);
	if(desc.isEmpty()) {
		return nullptr;
	}

	QString id = m_nextTemplateIds.take(idAlias);
	if(id.isEmpty()) {
		id = Ulid::make().toString();
	}

	SessionHistory *history = initHistory(
		id, idAlias,
		protocol::ProtocolVersion::fromString(
			desc[QStringLiteral("protocol")].toString()),
		desc[QStringLiteral("founder")].toString());

	if(!templateLoader()->init(history)) {
		delete history;
		return nullptr;
	}

	Session *session =
		new ThinSession(history, m_config, m_announcements, this);
	initSession(session);
	session->log(
		Log()
			.about(Log::Level::Info, Log::Topic::Status)
			.message(QStringLiteral("Session instantiated from template %1")
						 .arg(idAlias)));

	return session;
}

void SessionServer::initSession(Session *session)
{
	m_sessions.append(session);

	connect(
		session, &Session::sessionAttributeChanged, this,
		&SessionServer::onSessionAttributeChanged);
	connect(
		session, &Session::sessionDestroyed, this,
		&SessionServer::removeSession, Qt::DirectConnection);

	emit sessionCreated(session);
	emit sessionChanged(session->getDescription());
}

void SessionServer::removeSession(Session *session)
{
	m_sessions.removeOne(session);
	m_announcements->unlistSession(session); // just to be safe
	emit sessionEnded(session->id());
}

Session *SessionServer::getSessionById(const QString &id, bool load)
{
	for(Session *s : m_sessions) {
		if(s->id() == id || s->idAlias() == id) {
			return s;
		}
	}

	if(load) {
		const TemplateLoader *loader = templateLoader();
		if(loader) {
			if(loader->exists(id)) {
				return createFromTemplate(id);
			} else {
				for(QHash<QString, QString>::key_value_iterator
						it = m_nextTemplateIds.keyValueBegin(),
						end = m_nextTemplateIds.keyValueEnd();
					it != end; ++it) {
					if(it->second == id) {
						QString alias = it->first;
						if(loader->exists(alias)) {
							return createFromTemplate(alias);
						} else {
							m_nextTemplateIds.remove(alias);
							break;
						}
					}
				}
			}
		}
	}

	return nullptr;
}

Sessions::JoinResult SessionServer::checkSessionJoin(
	Client *client, const QString &idOrAlias, const QString &inviteSecret)
{
	for(Session *s : m_sessions) {
		QString id = s->id();
		if(id == idOrAlias || s->idAlias() == idOrAlias) {
			JoinResult result;
			result.id = id;
			result.description =
				s->getDescription(false, !inviteSecret.isEmpty());
			result.setInvite(s, client, inviteSecret);
			return result;
		}
	}

	const TemplateLoader *loader = templateLoader();
	if(loader && loader->exists(idOrAlias)) {
		QString id = m_nextTemplateIds.value(idOrAlias);
		if(id.isEmpty()) {
			id = Ulid::make().toString();
			m_nextTemplateIds.insert(idOrAlias, id);
		}
		JoinResult result;
		result.id = id;
		result.description = loader->templateDescription(idOrAlias);
		result.description.insert(QStringLiteral("id"), id);
		result.setInvite(nullptr, client, inviteSecret);
		return result;
	}

	return JoinResult();
}

void SessionServer::stopAll()
{
	for(ThinServerClient *c : m_clients) {
		// Note: this just sends the disconnect command, clients don't
		// self-delete immediately
		c->disconnectClient(
			Client::DisconnectionReason::Shutdown,
			QStringLiteral("Server shutting down"),
			QStringLiteral("SessionServer::stopAll"));
	}

	for(Session *s : m_sessions) {
		s->killSession(QStringLiteral("Server shutting down"), false);
	}
}

void SessionServer::messageAll(const QString &message, bool alert)
{
	for(Session *s : m_sessions) {
		s->messageAll(message, alert);
	}
}

void SessionServer::addClient(ThinServerClient *client)
{
	client->setParent(this);
	client->setConnectionTimeout(
		m_config->getConfigTime(config::ClientTimeout) * 1000);

	m_clients.append(client);
	connect(
		client, &ThinServerClient::thinServerClientDestroyed, this,
		&SessionServer::removeClient, Qt::DirectConnection);

	emit userCountChanged(m_clients.size());

	LoginHandler *login = new LoginHandler(client, this, m_config);
	connect(
		this, &SessionServer::sessionChanged, login,
		&LoginHandler::announceSession);
	connect(
		this, &SessionServer::sessionEnded, login,
		&LoginHandler::announceSessionEnd);
	login->startLoginProcess();
}

void SessionServer::removeClient(ThinServerClient *client)
{
	m_clients.removeOne(client);
	emit userCountChanged(m_clients.size());
}

/**
 * @brief Handle client disconnect from a session
 *
 * The session takes care of the client itself. Here, we clean up after the
 * session in case it needs to be closed.
 * @param session
 */
void SessionServer::onSessionAttributeChanged(Session *session)
{
	Q_ASSERT(session);

	bool delSession = false;

	if(session->isEffectivelyEmpty() &&
	   session->state() != Session::State::Shutdown) {
		session->log(
			Log()
				.about(Log::Level::Info, Log::Topic::Status)
				.message(QStringLiteral("Last user left.")));

		// A non-persistent session is deleted when the last user leaves
		// A persistent session can also be deleted if it doesn't contain a
		// snapshot point.
		if(!session->history()->hasFlag(SessionHistory::Persistent) &&
		   m_config->getConfigTime(config::EmptySessionLingerTime) <= 0) {
			session->log(
				Log()
					.about(Log::Level::Info, Log::Topic::Status)
					.message(
						QStringLiteral("Closing non-persistent session.")));
			delSession = true;
		}
	}

	if(delSession) {
		session->killSession(
			QStringLiteral("Session terminated due to being empty"));
	} else {
		emit sessionChanged(session->getDescription());
	}
}

void SessionServer::cleanupSessions()
{
	qint64 emptySessionLingerTime =
		m_config->getConfigTime(config::EmptySessionLingerTime) * 1000;
	qint64 expirationTime =
		m_config->getConfigTime(config::IdleTimeLimit) * 1000;
	bool allowIdleOverride =
		expirationTime > 0 ? m_config->getConfigBool(config::AllowIdleOverride)
						   : false;
	for(Session *s : m_sessions) {
		qint64 lastEventTime = s->lastEventTime();
		if(!s->history()->hasFlag(SessionHistory::Persistent) &&
		   s->isEffectivelyEmpty() && lastEventTime > emptySessionLingerTime) {
			s->log(
				Log()
					.about(Log::Level::Info, Log::Topic::Status)
					.message(QStringLiteral(
						"Closing lingering non-persistent session.")));
			s->killSession(QStringLiteral(
				"Session terminated due to being empty too long"));
		} else if(
			expirationTime > 0 && lastEventTime > expirationTime &&
			(!allowIdleOverride ||
			 !s->history()->hasFlag(SessionHistory::IdleOverride))) {
			s->log(
				Log()
					.about(Log::Level::Info, Log::Topic::Status)
					.message(QStringLiteral("Idle session expired.")));
			s->killSession(QStringLiteral(
				"Session terminated due to being idle too long"));
		}
	}
}

JsonApiResult SessionServer::callSessionJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request,
	bool sectionLocked)
{
	QString head;
	QStringList tail;
	std::tie(head, tail) = popApiPath(path);

	if(!head.isEmpty()) {
		Session *s = getSessionById(head, false);
		if(s) {
			return s->callJsonApi(method, tail, request, sectionLocked);
		} else {
			return JsonApiNotFound();
		}
	}

	if(method == JsonApiMethod::Get) {
		QJsonDocument body;
		bool onlyListed =
			parseRequestBool(request, QStringLiteral("listed"), 0, 0);
		if(parseRequestInt(request, QStringLiteral("v"), 0, 0) <= 1) {
			body.setArray(sessionDescriptions(!onlyListed));
		} else {
			body.setObject({
				{QStringLiteral("sessions"), sessionDescriptions(!onlyListed)},
				{QStringLiteral("_locked"), sectionLocked},
			});
		}
		return JsonApiResult{JsonApiResult::Ok, body};
	} else if(method == JsonApiMethod::Update) {
		QString msg = request[QStringLiteral("message")].toString();
		if(!msg.isEmpty()) {
			messageAll(msg, false);
		}

		QString alert = request[QStringLiteral("alert")].toString();
		if(!alert.isEmpty()) {
			messageAll(alert, true);
		}

		return {JsonApiResult::Ok, QJsonDocument(QJsonObject())};
	} else {
		return JsonApiBadMethod();
	}
}

JsonApiResult SessionServer::callUserJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request,
	bool sectionLocked)
{
	if(method == JsonApiMethod::Get) {
		switch(path.size()) {
		case 0: {
			QJsonArray userlist;
			for(const ThinServerClient *c : m_clients) {
				userlist.append(c->description());
			}
			QJsonDocument body;
			if(parseRequestInt(request, QStringLiteral("v"), 0, 0) <= 1) {
				body.setArray(userlist);
			} else {
				body.setObject({
					{QStringLiteral("users"), userlist},
					{QStringLiteral("_locked"), sectionLocked},
				});
			}
			return JsonApiResult{JsonApiResult::Ok, body};
		}
		case 1: {
			ThinServerClient *c = searchClientByPathUid(path[0]);
			if(c) {
				QJsonObject body = c->description();
				body.insert(QStringLiteral("_locked"), sectionLocked);
				return {JsonApiResult::Ok, QJsonDocument()};
			} else {
				return JsonApiNotFound();
			}
		}
		default:
			return JsonApiNotFound();
		}

	} else if(method == JsonApiMethod::Delete) {
		if(path.size() == 1) {
			ThinServerClient *c = searchClientByPathUid(path[0]);
			if(c) {
				return c->jsonApiKick(
					request[QStringLiteral("message")].toString());
			}
		}
		return JsonApiNotFound();

	} else {
		return JsonApiBadMethod();
	}
}

ThinServerClient *SessionServer::searchClientByPathUid(const QString &uid)
{
	for(ThinServerClient *c : m_clients) {
		if(uid == c->uid()) {
			return c;
		}
	}
	return nullptr;
}

}
