// SPDX-License-Identifier: GPL-3.0-or-later

#include "libserver/opcommands.h"
#include "libserver/client.h"
#include "libserver/session.h"
#include "libserver/serverlog.h"
#include "libshared/net/control.h"
#include "libshared/net/meta.h"
#include "libshared/util/passwordhash.h"

#include <QList>
#include <QStringList>
#include <QUrl>

namespace server {

namespace {

class CmdError {
public:
	CmdError(const QString &msg) : m_msg(msg) {}

	const QString &message() const { return m_msg; }

private:
	QString m_msg;
};

typedef void (*SrvCommandFn)(Client *, const QJsonArray &, const QJsonObject &);

class SrvCommand {
public:
	enum Mode {
		NONOP,  // usable by all
		DEPUTY, // needs at least deputy privileges
		OP,     // needs operator privileges
		MOD     // needs moderator privileges
	};

	SrvCommand(const QString &name, SrvCommandFn fn, Mode mode=OP)
		: m_fn(fn), m_name(name), m_mode(mode)
	{}

	void call(Client *c, const QJsonArray &args, const QJsonObject &kwargs) const { m_fn(c, args, kwargs); }
	const QString &name() const { return m_name; }

	Mode mode() const { return m_mode; }

private:
	SrvCommandFn m_fn;
	QString m_name;
	Mode m_mode;
};

struct SrvCommandSet {
	QList<SrvCommand> commands;

	SrvCommandSet();
};

const SrvCommandSet COMMANDS;

void readyToAutoReset(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);
	client->session()->readyToAutoReset(client->id());
}

void initBegin(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);
	client->session()->handleInitBegin(client->id());
}

void initComplete(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);
	client->session()->handleInitComplete(client->id());
}

void initCancel(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);
	client->session()->handleInitCancel(client->id());
}

void sessionConf(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	client->session()->setSessionConfig(kwargs, client);
}

void opWord(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(kwargs);
	if(args.size() != 1)
		throw CmdError("Expected one argument: opword");

	const QByteArray opwordHash = client->session()->history()->opwordHash();
	if(opwordHash.isEmpty())
		throw CmdError("No opword set");

	if(passwordhash::check(args.at(0).toString(), opwordHash)) {
		client->session()->changeOpStatus(client->id(), true, "password");

	} else {
		throw CmdError("Incorrect password");
	}
}

Client *_getClient(Session *session, const QJsonValue &idOrName)
{
	Client *c = nullptr;
	if(idOrName.isDouble()) {
		// ID number
		const int id = idOrName.toInt();
		if(id<1 || id > 254)
			throw CmdError("invalid user id: " + QString::number(id));
		c = session->getClientById(id);

	} else if(idOrName.isString()){
		// Username
		c = session->getClientByUsername(idOrName.toString());
	} else {
		throw CmdError("invalid user ID or name");
	}
	if(!c)
		throw CmdError("user not found");

	return c;
}

void kickUser(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	if(args.size()!=1)
		throw CmdError("Expected one argument: user ID or name");

	const bool ban = kwargs["ban"].toBool();

	if(ban && client->session()->hasPastClientWithId(args.at(0).toInt())) {
		// Retroactive ban
		const auto target = client->session()->getPastClientById(args.at(0).toInt());
		if(target.isBannable) {
			client->session()->addBan(target, client->username());
			client->session()->messageAll(target.username + " banned by " + client->username(), false);
		} else {
			throw CmdError(target.username + " cannot be banned.");
		}
		return;
	}

	Client *target = _getClient(client->session(), args.at(0));
	if(target == client)
		throw CmdError("cannot kick self");

	if(target->isModerator())
		throw CmdError("cannot kick moderators");

	if(client->isDeputy()) {
		if(target->isOperator() || target->isTrusted())
			throw CmdError("cannot kick trusted users");
	}

	if(ban) {
		client->session()->addBan(target, client->username());
		client->session()->messageAll(target->username() + " banned by " + client->username(), false);
	} else {
		client->session()->messageAll(target->username() + " kicked by " + client->username(), false);
	}

	target->disconnectClient(Client::DisconnectionReason::Kick, client->username());
}

void removeBan(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(kwargs);
	if(args.size()!=1)
		throw CmdError("Expected one argument: ban entry ID");

	client->session()->removeBan(args.at(0).toInt(), client->username());
}

void killSession(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);

	client->session()->messageAll(QString("Session shut down by moderator (%1)").arg(client->username()), true);
	client->session()->killSession();
}

void announceSession(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	if(args.size()!=1)
		throw CmdError("Expected one argument: API URL");

	QUrl apiUrl { args.at(0).toString() };
	if(!apiUrl.isValid())
		throw CmdError("Invalid API URL");

	client->session()->makeAnnouncement(apiUrl, kwargs["private"].toBool());
}

void unlistSession(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(kwargs);
	if(args.size() != 1)
		throw CmdError("Expected one argument: API URL");

	client->session()->unlistAnnouncement(args.at(0).toString());
}

void resetSession(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);

	if(client->session()->state() != Session::State::Running)
		throw CmdError("Unable to reset in this state");

	client->session()->resetSession(client->id());
}

void setMute(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(kwargs);

	if(args.size() != 2)
		throw CmdError("Expected two arguments: userId true/false");

	Client *c = _getClient(client->session(), args.at(0));

	const bool m = args.at(1).toBool();
	if(c->isMuted() != m) {
		c->setMuted(m);
		client->session()->sendUpdatedMuteList();
		if(m)
			c->log(Log().about(Log::Level::Info, Log::Topic::Mute).message("Muted by " + client->username()));
		else
			c->log(Log().about(Log::Level::Info, Log::Topic::Unmute).message("Unmuted by " + client->username()));
	}
}

void reportAbuse(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);

	const int user = kwargs["user"].toInt();
	const QString reason = kwargs["reason"].toString();

	client->session()->sendAbuseReport(client, user, reason);
}

SrvCommandSet::SrvCommandSet()
{
	commands
		<< SrvCommand("ready-to-autoreset", readyToAutoReset)
		<< SrvCommand("init-begin", initBegin)
		<< SrvCommand("init-complete", initComplete)
		<< SrvCommand("init-cancel", initCancel)
		<< SrvCommand("sessionconf", sessionConf)
		<< SrvCommand("kick-user", kickUser, SrvCommand::DEPUTY)
		<< SrvCommand("gain-op", opWord, SrvCommand::NONOP)

		<< SrvCommand("reset-session", resetSession)
		<< SrvCommand("kill-session", killSession, SrvCommand::MOD)

		<< SrvCommand("announce-session", announceSession)
		<< SrvCommand("unlist-session", unlistSession)

		<< SrvCommand("remove-ban", removeBan)
		<< SrvCommand("mute", setMute)

		<< SrvCommand("report", reportAbuse, SrvCommand::NONOP)
	;
}

} // end of anonymous namespace

void handleClientServerCommand(Client *client, const QString &command, const QJsonArray &args, const QJsonObject &kwargs)
{
	for(const SrvCommand &c : COMMANDS.commands) {
		if(c.name() == command) {
			if(c.mode() == SrvCommand::MOD && !client->isModerator()) {
				client->sendDirectMessage(protocol::Command::error(command + ": Not a moderator"));
				return;
			}
			else if(c.mode() == SrvCommand::OP && !client->isOperator()) {
				client->sendDirectMessage(protocol::Command::error(command + ": Not a session owner"));
				return;
			}
			else if(c.mode() == SrvCommand::DEPUTY && !client->isOperator() && !client->isDeputy()) {
				client->sendDirectMessage(protocol::Command::error(command + ": Not a session owner or a deputy"));
				return;
			}

			try {
				c.call(client, args, kwargs);
			} catch(const CmdError &err) {

				protocol::ServerReply reply;
				reply.type = protocol::ServerReply::ERROR;
				reply.message = err.message();
				client->sendDirectMessage(protocol::Command::error(err.message()));
			}
			return;
		}
	}

	client->sendDirectMessage(protocol::Command::error("Unknown command: " + command));
}

}
