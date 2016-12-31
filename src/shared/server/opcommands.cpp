/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2015 Calle Laakkonen

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

void sendResult(Client *client, const QString &result)
{
	protocol::ServerReply reply;
	reply.type = protocol::ServerReply::RESULT;
	reply.message = result;
	client->sendDirectMessage(protocol::MessagePtr(new protocol::Command(0, reply)));
}

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

Client *_getClient(Client *me, const QString &name)
{
	Client *c = nullptr;
	if(name.startsWith("#")) {
		// ID number
		bool ok;
		int id = name.mid(1).toInt(&ok);
		if(!ok)
			throw CmdError("invalid user id: " + name);
		c = me->session()->getClientById(id);

	} else {
		// Username
		c = me->session()->getClientByUsername(name);
	}
	if(!c)
		throw CmdError("no such user: " + name);

	return c;
}

void kickUser(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(kwargs);
	if(args.size()!=1)
		throw CmdError("Expected one argument: user #ID or name");

	Client *target = _getClient(client, args.at(0).toString());
	if(target == client)
		throw CmdError("cannot kick self");

	if(target->isModerator())
		throw CmdError("cannot kick moderators");

	target->disconnectKick(client->username());
}

void listUsers(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);

	QString msg("Users:\n");

	for(const Client *c : client->session()->clients()) {
		QString flags;
		if(c->isModerator())
			flags = "M";
		else if(c->isOperator())
			flags = "@";

		if(c->isAuthenticated())
			flags += "A";

		msg.append(QString("#%1: %2 [%3]\n").arg(c->id()).arg(c->username(), flags));
	}
	sendResult(client, msg);
}

void sessionStatus(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);

	const Session *s = client->session();
	QString msg;
	msg.append(QString("%1, up %2\n").arg(s->toLogString()).arg(s->uptime()));
	msg.append(QString("Logged in users: %1 (max: %2)\n").arg(s->userCount()).arg(s->maxUsers()));
	msg.append(QString("Persistent session: %1\n").arg(s->isPersistent() ? "yes" : "no"));
	msg.append(QString("Password protected: %1\n").arg(s->hasPassword() ? "yes" : "no"));
	msg.append(QString("History size: %1 Mb (limit: %2 Mb)\n")
			.arg(s->mainstream().lengthInBytes() / qreal(1024*1024), 0, 'f', 2)
			.arg(s->historyLimit() / qreal(1024*1024), 0, 'f', 2));
	msg.append(QString("History indices: %1 -- %2\n").arg(s->mainstream().offset()).arg(s->mainstream().end()));

	sendResult(client, msg);
}

void killSession(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);

	client->session()->wall(QString("Session shut down by moderator (%1)").arg(client->username()));
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
	client->session()->addToCommandStream(protocol::MessagePtr(new protocol::Command(0, chat)));
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

		<< SrvCommand("who", listUsers)
		<< SrvCommand("status", sessionStatus)
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
