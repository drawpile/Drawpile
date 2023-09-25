// SPDX-License-Identifier: GPL-3.0-or-later
#include "libserver/opcommands.h"
#include "libserver/client.h"
#include "libserver/serverlog.h"
#include "libserver/session.h"
#include "libshared/net/servercmd.h"
#include "libshared/util/passwordhash.h"
#include <QJsonArray>
#include <QList>
#include <QStringList>
#include <QUrl>

namespace server {

namespace {

class CmdResult {
public:
	static CmdResult ok() { return CmdResult{true, QString{}}; }

	static CmdResult err(const QString &message)
	{
		return CmdResult{false, message};
	}

	bool success() const { return m_success; }
	const QString &message() const { return m_message; }

private:
	CmdResult(bool success, const QString &message)
		: m_success(success)
		, m_message(message)
	{
	}

	bool m_success;
	QString m_message;
};

typedef CmdResult (*SrvCommandFn)(
	Client *, const QJsonArray &, const QJsonObject &);

class SrvCommand {
public:
	enum Mode {
		NONOP,	// usable by all
		DEPUTY, // needs at least deputy privileges
		OP,		// needs operator privileges
		MOD		// needs moderator privileges
	};

	SrvCommand(const QString &name, SrvCommandFn fn, Mode mode = OP)
		: m_fn(fn)
		, m_name(name)
		, m_mode(mode)
	{
	}

	CmdResult
	call(Client *c, const QJsonArray &args, const QJsonObject &kwargs) const
	{
		return m_fn(c, args, kwargs);
	}

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

CmdResult readyToAutoReset(
	Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);
	client->session()->readyToAutoReset(client->id());
	return CmdResult::ok();
}

CmdResult
initBegin(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);
	client->session()->handleInitBegin(client->id());
	return CmdResult::ok();
}

CmdResult
initComplete(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);
	client->session()->handleInitComplete(client->id());
	return CmdResult::ok();
}

CmdResult
initCancel(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);
	client->session()->handleInitCancel(client->id());
	return CmdResult::ok();
}

CmdResult
sessionConf(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	client->session()->setSessionConfig(kwargs, client);
	return CmdResult::ok();
}

CmdResult
opWord(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(kwargs);
	if(args.size() != 1)
		return CmdResult::err("Expected one argument: opword");

	const QByteArray opwordHash = client->session()->history()->opwordHash();
	if(opwordHash.isEmpty())
		return CmdResult::err("No opword set");

	if(passwordhash::check(args.at(0).toString(), opwordHash)) {
		client->session()->changeOpStatus(client->id(), true, "password");
		return CmdResult::ok();

	} else {
		return CmdResult::err("Incorrect password");
	}
}

static CmdResult
getClient(Session *session, const QJsonValue &idOrName, Client *&outClient)
{
	Client *c;
	if(idOrName.isDouble()) {
		// ID number
		const int id = idOrName.toInt();
		if(id < 1 || id > 254)
			return CmdResult::err("invalid user id: " + QString::number(id));
		c = session->getClientById(id);

	} else if(idOrName.isString()) {
		// Username
		c = session->getClientByUsername(idOrName.toString());
	} else {
		return CmdResult::err("invalid user ID or name");
	}

	if(c) {
		outClient = c;
		return CmdResult::ok();
	} else {
		return CmdResult::err("user not found");
	}
}

CmdResult
kickUser(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	if(args.size() != 1)
		return CmdResult::err("Expected one argument: user ID or name");

	const bool ban = kwargs["ban"].toBool();

	if(ban && client->session()->hasPastClientWithId(args.at(0).toInt())) {
		// Retroactive ban
		const auto target =
			client->session()->getPastClientById(args.at(0).toInt());
		if(target.isBannable) {
			client->session()->addBan(target, client->username());
			client->session()->keyMessageAll(
				target.username + " banned by " + client->username(), false,
				net::ServerReply::KEY_BAN,
				{{QStringLiteral("target"), target.username},
				 {QStringLiteral("by"), client->username()}});
			return CmdResult::ok();
		} else {
			return CmdResult::err(target.username + " cannot be banned.");
		}
	}

	Client *target;
	CmdResult result = getClient(client->session(), args.at(0), target);
	if(!result.success()) {
		return result;
	}

	if(target == client)
		return CmdResult::err("cannot kick self");

	if(target->isModerator())
		return CmdResult::err("cannot kick moderators");

	if(client->isDeputy()) {
		if(target->isOperator() || target->isTrusted())
			return CmdResult::err("cannot kick trusted users");
	}

	if(ban) {
		client->session()->addBan(target, client->username());
		client->session()->keyMessageAll(
			target->username() + " banned by " + client->username(), false,
			net::ServerReply::KEY_BAN,
			{{QStringLiteral("target"), target->username()},
			 {QStringLiteral("by"), client->username()}});
	} else {
		client->session()->keyMessageAll(
			target->username() + " kicked by " + client->username(), false,
			net::ServerReply::KEY_KICK,
			{{QStringLiteral("target"), target->username()},
			 {QStringLiteral("by"), client->username()}});
	}

	target->disconnectClient(
		Client::DisconnectionReason::Kick, client->username());
	return CmdResult::ok();
}

CmdResult
removeBan(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(kwargs);
	if(args.size() != 1)
		return CmdResult::err("Expected one argument: ban entry ID");

	client->session()->removeBan(args.at(0).toInt(), client->username());
	return CmdResult::ok();
}

CmdResult
killSession(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);

	client->session()->keyMessageAll(
		QStringLiteral("Session shut down by moderator (%1)")
			.arg(client->username()),
		true, net::ServerReply::KEY_TERMINATE_SESSION,
		{{QStringLiteral("by"), client->username()}});
	client->session()->killSession();
	return CmdResult::ok();
}

CmdResult announceSession(
	Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	if(args.size() != 1)
		return CmdResult::err("Expected one argument: API URL");

	QUrl apiUrl{args.at(0).toString()};
	if(!apiUrl.isValid())
		return CmdResult::err("Invalid API URL");

	client->session()->makeAnnouncement(apiUrl, kwargs["private"].toBool());
	return CmdResult::ok();
}

CmdResult
unlistSession(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(kwargs);
	if(args.size() != 1)
		return CmdResult::err("Expected one argument: API URL");

	client->session()->unlistAnnouncement(args.at(0).toString());
	return CmdResult::ok();
}

CmdResult
resetSession(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);

	if(client->session()->state() != Session::State::Running)
		return CmdResult::err("Unable to reset in this state");

	client->session()->resetSession(client->id());
	return CmdResult::ok();
}

CmdResult
setMute(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(kwargs);

	if(args.size() != 2)
		return CmdResult::err("Expected two arguments: userId true/false");

	Client *c;
	CmdResult result = getClient(client->session(), args.at(0), c);
	if(!result.success()) {
		return result;
	}

	const bool m = args.at(1).toBool();
	if(c->isMuted() != m) {
		c->setMuted(m);
		client->session()->sendUpdatedMuteList();
		if(m)
			c->log(Log()
					   .about(Log::Level::Info, Log::Topic::Mute)
					   .message("Muted by " + client->username()));
		else
			c->log(Log()
					   .about(Log::Level::Info, Log::Topic::Unmute)
					   .message("Unmuted by " + client->username()));
	}
	return CmdResult::ok();
}

CmdResult
reportAbuse(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);

	const int user = kwargs["user"].toInt();
	const QString reason = kwargs["reason"].toString();

	client->session()->sendAbuseReport(client, user, reason);
	return CmdResult::ok();
}

SrvCommandSet::SrvCommandSet()
{
	commands << SrvCommand("ready-to-autoreset", readyToAutoReset)
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

			 << SrvCommand("report", reportAbuse, SrvCommand::NONOP);
}

} // end of anonymous namespace

void handleClientServerCommand(
	Client *client, const QString &command, const QJsonArray &args,
	const QJsonObject &kwargs)
{
	for(const SrvCommand &c : COMMANDS.commands) {
		if(c.name() == command) {
			if(c.mode() == SrvCommand::MOD && !client->isModerator()) {
				client->sendDirectMessage(net::ServerReply::makeCommandError(
					command, QStringLiteral("Not a moderator")));
				return;
			} else if(c.mode() == SrvCommand::OP && !client->isOperator()) {
				client->sendDirectMessage(net::ServerReply::makeCommandError(
					command, QStringLiteral("Not a session owner")));
				return;
			} else if(
				c.mode() == SrvCommand::DEPUTY && !client->isOperator() &&
				!client->isDeputy()) {
				client->sendDirectMessage(net::ServerReply::makeCommandError(
					command,
					QStringLiteral("Not a session owner or a deputy")));
				return;
			}

			CmdResult result = c.call(client, args, kwargs);
			if(!result.success()) {
				client->sendDirectMessage(net::ServerReply::makeCommandError(
					command, result.message()));
			}
			return;
		}
	}

	client->sendDirectMessage(
		net::ServerReply::makeCommandError(command, "Unknown command"));
}

}
