/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2017 Calle Laakkonen

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

#include "opcommands.h"
#include "client.h"
#include "session.h"
//#include "../net/meta.h"
#include "../net/control.h"

#include <QList>
#include <QStringList>

namespace server {

namespace {

class CmdError {
public:
	CmdError() {}
	CmdError(const QString &msg) : m_msg(msg) {}

	const QString &message() const { return m_msg; }

private:
	QString m_msg;
};

typedef void (*SrvCommandFn)(Client *, const QJsonArray &, const QJsonObject &);

class SrvCommand {
public:
	SrvCommand(const QString &name, SrvCommandFn fn)
		: m_fn(fn), m_name(name), m_opOnly(true), m_modOnly(false)
	{}

	void call(Client *c, const QJsonArray &args, const QJsonObject &kwargs) const { m_fn(c, args, kwargs); }
	const QString &name() const { return m_name; }

	bool isModOnly() const { return m_modOnly; }
	bool isOpOnly() const { return m_opOnly; }

	SrvCommand &nonOp() { m_opOnly = false; return *this; }
	SrvCommand &modOnly() { m_modOnly = true; return *this; }

private:
	SrvCommandFn m_fn;
	QString m_name;
	bool m_opOnly;
	bool m_modOnly;
};

struct SrvCommandSet {
	QList<SrvCommand> commands;

	SrvCommandSet();
};

const SrvCommandSet COMMANDS;

void initComplete(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);
	client->session()->handleInitComplete(client->id());
}

void sessionConf(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	client->session()->setSessionConfig(kwargs);
}

void getBanList(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);

	// The banlist is not usually included in the sessionconf.
	// Moderators and local users get to see the actual IP addresses too
	protocol::ServerReply msg;
	msg.type = protocol::ServerReply::SESSIONCONF;
	QJsonObject conf;
	conf["banlist"]= client->session()->banlist().toJson(client->isModerator() || client->peerAddress().isLoopback());
	msg.reply["config"] = conf;
	client->sendDirectMessage(protocol::MessagePtr(new protocol::Command(0, msg)));
}

void removeBan(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(kwargs);
	if(args.size()!=1)
		throw CmdError("Expected one argument: ban entry ID");

	client->session()->removeBan(args.at(0).toInt(), client->username());
	getBanList(client, QJsonArray(), QJsonObject());
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

	Client *target = _getClient(client->session(), args.at(0));
	if(target == client)
		throw CmdError("cannot kick self");

	if(target->isModerator())
		throw CmdError("cannot kick moderators");

	if(kwargs["ban"].toBool()) {
		client->session()->addBan(target, client->username());
		getBanList(client, QJsonArray(), QJsonObject());
	}

	target->disconnectKick(client->username());
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
	Q_UNUSED(kwargs);
	if(args.size()!=1)
		throw CmdError("Expected one argument: API URL");

	QUrl apiUrl(args.at(0).toString());
	if(!apiUrl.isValid())
		throw CmdError("Expected one argument: API URL");

	if(client->session()->publicListing().listingId>0)
		// TODO support announcing at multiple sites simultaneously
		throw CmdError("Expected one argument: API URL");

	client->session()->makeAnnouncement(apiUrl);
}

void unlistSession(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);
	if(client->session()->publicListing().listingId<=0) {
		throw CmdError("Expected one argument: API URL");
		return;
	}

	client->session()->unlistAnnouncement();
}

void chatMessage(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	if(args.size()!=1)
		throw CmdError("Expected one argument: chat message");

	protocol::ServerReply chat;
	chat.type = protocol::ServerReply::CHAT;
	chat.message = args.at(0).toString();
	chat.reply["user"] = client->id();
	if(!kwargs.isEmpty())
		chat.reply["options"] = kwargs;
	client->session()->directToAll(protocol::MessagePtr(new protocol::Command(0, chat)));
}

void resetSession(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);

	if(client->session()->state() != Session::Running)
		throw CmdError("Unable to reset in this state");

	client->session()->resetSession(client->id());
}

SrvCommandSet::SrvCommandSet()
{
	commands
		<< SrvCommand("init-complete", initComplete)
		<< SrvCommand("sessionconf", sessionConf)
		<< SrvCommand("kick-user", kickUser)

		<< SrvCommand("reset-session", resetSession)
		<< SrvCommand("kill-session", killSession).modOnly()

		<< SrvCommand("announce-session", announceSession)
		<< SrvCommand("unlist-session", unlistSession)

		<< SrvCommand("get-banlist", getBanList).nonOp()
		<< SrvCommand("remove-ban", removeBan)

		<< SrvCommand("chat", chatMessage).nonOp()
	;
}

} // end of anonymous namespace

void handleClientServerCommand(Client *client, const QString &command, const QJsonArray &args, const QJsonObject &kwargs)
{
	for(const SrvCommand &c : COMMANDS.commands) {
		if(c.name() == command) {
			if(c.isModOnly() && !client->isModerator()) {
				client->sendDirectMessage(protocol::Command::error("Not a moderator"));
				return;
			}
			if(c.isOpOnly() && !client->isOperator()) {
				client->sendDirectMessage(protocol::Command::error("Not a session owner"));
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
