/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
#include "../net/meta.h"

#include <QList>
#include <QStringList>

namespace server {

namespace {

class OpError {
public:
	OpError() {}
	OpError(const QString &msg) : _msg(msg) {}

	const QString &message() const { return _msg; }

private:
	QString _msg;
};

typedef void (*OpCommandFn)(Client *, const QString &, const QStringList &);

class OpCommand {
public:
	OpCommand(const QString &name, OpCommandFn fn, const QString &paramsDesc, const QString &description, int minargs, int maxargs)
		: _fn(fn), _name(name), _paramsDescription(paramsDesc), _description(description), _minargs(minargs), _maxargs(maxargs)
	{}
	OpCommand(const QString &name, OpCommandFn fn, const QString &paramsDesc, const QString &description, int args=0)
		: OpCommand(name, fn, paramsDesc, description, args, args)
	{}

	void call(Client *c, const QString &cmd, const QStringList &tokens) const { _fn(c, cmd, tokens); }
	const QString &name() const { return _name; }
	const QString &paramsDescription() const { return _paramsDescription; }
	const QString &description() const { return _description; }

	bool checkParamCount(int count) const {
		return count >= _minargs && (_maxargs<0 || count <= _maxargs);
	}

private:
	OpCommandFn _fn;
	QString _name;
	QString _paramsDescription;
	QString _description;
	int _minargs;
	int _maxargs;
};

struct OpCommandSet {
	QList<OpCommand> commands;

	OpCommandSet();
};

const OpCommandSet COMMANDS;

bool _getOnOff(const QString &value) {
	if(value.compare("on", Qt::CaseInsensitive)==0)
		return true;
	else if(value.compare("off", Qt::CaseInsensitive)==0)
		return false;

	throw OpError("invalid on/off value: " + value);
}

void lockBoard(Client *client, const QString &, const QStringList &tokens)
{
	client->session()->setLocked(tokens.size()==1 || _getOnOff(tokens.at(1)));
}

void lockLayerCtrls(Client *client, const QString &, const QStringList &tokens)
{
	client->session()->setLayerControlLocked(tokens.size()==1 || _getOnOff(tokens.at(1)));
}

void loginsOpen(Client *client, const QString &, const QStringList &tokens)
{
	client->session()->setClosed(!_getOnOff(tokens.at(1)));
}

void lockDefault(Client *client, const QString &, const QStringList &tokens)
{
	client->session()->setUsersLockedByDefault(tokens.size()==1 || _getOnOff(tokens.at(1)));
}

void setSessionTitle(Client *client, const QString &, const QStringList &tokens)
{
	QString title = QStringList(tokens.mid(1)).join(' ');
	client->session()->setTitle(title);
	client->session()->addToCommandStream(protocol::MessagePtr(new protocol::SessionTitle(client->id(), title)));
}

void setMaxUsers(Client *client, const QString &, const QStringList &tokens)
{
	bool ok;
	int limit = tokens[1].toInt(&ok);
	if(ok && limit>0)
		client->session()->setMaxUsers(limit);
	else
		throw OpError("Invalid user limit: " + tokens[1]);
}

void setPassword(Client *client, const QString &cmd, const QStringList &tokens)
{
	if(tokens.length()==1)
		client->session()->setPassword(QString());
	else // note: password may contain spaces
		client->session()->setPassword(cmd.mid(cmd.indexOf(' ') + 1));
}

void persistSession(Client *client, const QString &, const QStringList &tokens)
{
	client->session()->setPersistent(tokens.size()==1 || _getOnOff(tokens.at(1)));
}

Client *_getClient(Client *me, const QString &name)
{
	Client *c = nullptr;
	if(name.startsWith("#")) {
		// ID number
		bool ok;
		int id = name.mid(1).toInt(&ok);
		if(!ok)
			throw OpError("invalid user id: " + name);
		c = me->session()->getClientById(id);

	} else {
		// Username
		c = me->session()->getClientByUsername(name);
	}
	if(!c)
		throw OpError("no such user: " + name);

	return c;
}

void kickUser(Client *client, const QString &, const QStringList &tokens)
{
	Client *target = _getClient(client, tokens.at(1));
	if(target == client)
		throw OpError("cannot kick self");

	target->disconnectKick(client->username());
}

void opUser(Client *client, const QString &, const QStringList &tokens)
{
	Client *target = _getClient(client, tokens.at(1));
	if(!target->isOperator())
		target->grantOp();
}

void deopUser(Client *client, const QString &, const QStringList &tokens)
{
	Client *target = _getClient(client, tokens.at(1));
	if(target==client)
		throw OpError("cannot deop self");
	target->deOp();
}

void lockUser(Client *client, const QString &, const QStringList &tokens)
{
	Client *target = _getClient(client, tokens.at(1));
	target->lockUser();
}

void unlockUser(Client *client, const QString &, const QStringList &tokens)
{
	Client *target = _getClient(client, tokens.at(1));
	target->unlockUser();
}

void listUsers(Client *client, const QString &, const QStringList &)
{
	QString msg("Users:\n");

	for(const Client *c : client->session()->clients()) {
		QString flags;
		if(c->isOperator())
			flags = "@";
		if(c->isUserLocked())
			flags += "L";
		if(c->isHoldLocked())
			flags += "l";
		msg.append(QString("#%1: %2 [%3]\n").arg(c->id()).arg(c->username(), flags));
	}
	client->sendSystemChat(msg);
}

void sessionStatus(Client *client, const QString &, const QStringList &)
{
	const SessionState *s = client->session();
	QString msg;
	msg.append(QString("Session #%1, up %2\n").arg(s->id()).arg(s->uptime()));
	msg.append(QString("Logged in users: %1 (max: %2)\n").arg(s->userCount()).arg(s->maxUsers()));
	msg.append(QString("Persistent session: %1\n").arg(s->isPersistenceAllowed() ? (s->isPersistent() ? "yes" : "no") : "not allowed"));
	msg.append(QString("Password protected: %1\n").arg(s->password().isEmpty() ? "no" : "yes"));
	msg.append(QString("History size: %1 Mb (limit: %2 Mb)\n")
			.arg(s->mainstream().lengthInBytes() / qreal(1024*1024), 0, 'f', 2)
			.arg(s->historyLimit() / qreal(1024*1024), 0, 'f', 2));
	msg.append(QString("History indices: %1 -- %2\n").arg(s->mainstream().offset()).arg(s->mainstream().end()));
	msg.append(QString("Snapshot point exists: %1").arg(s->mainstream().hasSnapshot() ? "yes" : "no"));

	client->sendSystemChat(msg);
}

void showHelp(Client *client, const QString &, const QStringList &)
{
	QString message("Supported commands:\n");

	// calculate padding for paramsDescription
	int pdLen=0;
	for(const OpCommand &c : COMMANDS.commands) {
		int len = c.name().length();
		if(!c.paramsDescription().isEmpty())
			len += c.paramsDescription().length() + 1;
		pdLen = qMax(pdLen, len);
	}

	// Generate help text
	for(const OpCommand &c : COMMANDS.commands) {
		message.append("/");

		int padding = c.name().length();
		message.append(c.name());
		if(!c.paramsDescription().isEmpty()) {
			message.append(' ');
			message.append(c.paramsDescription());
			padding += c.paramsDescription().length() + 1;
		}
		padding = pdLen - padding + 1;
		while(padding--) message.append(' ');

		message.append(c.description());
		message.append('\n');
	}

	client->sendSystemChat(message);
}

OpCommandSet::OpCommandSet()
{
	const QString ID = QStringLiteral("<#id/name>");
	const QString ONOFF = QStringLiteral("[on/off]");
	const QString EONOFF = QStringLiteral("<on/off>");

	commands
		<< OpCommand("lockboard", lockBoard, ONOFF, "lock the drawing board", 0, 1)
		<< OpCommand("locklayerctrl", lockLayerCtrls, ONOFF, "lock layer controls", 0, 1)
		<< OpCommand("logins", loginsOpen, EONOFF, "enable/disable logins", 1)
		<< OpCommand("lockdefault", lockDefault, ONOFF, "automatically lock new users", 0, 1)
		<< OpCommand("persistence", persistSession, ONOFF, "make session persistent", 0, 1)

		<< OpCommand("maxusers", setMaxUsers, "<count>", "set user limit", 1)
		<< OpCommand("title", setSessionTitle, "[title]", "set session title", 0, -1)
		<< OpCommand("password", setPassword, "[password]", "set session password", 0, -1)

		<< OpCommand("kick", kickUser, ID, "remove user from session", 1)
		<< OpCommand("op", opUser, ID, "make session operator", 1)
		<< OpCommand("deop", deopUser, ID, "revoke operator privileges", 1)
		<< OpCommand("lock", lockUser, ID, "lock user", 1)
		<< OpCommand("unlock", unlockUser, ID, "unlock user", 1)

#ifndef NDEBUG
		<< OpCommand("force_snapshot", [](Client *c, const QString&, const QStringList&) { c->session()->startSnapshotSync(); }, QString(), "force snapshot sync")
#endif

		<< OpCommand("who", listUsers, QString(), "list logged in users")
		<< OpCommand("status", sessionStatus, QString(), "show session status")
		<< OpCommand("help", showHelp, QString(), "show this help text")
	;
}

} // end of anonymous namespace

void handleSessionOperatorCommand(Client *client, const QString &command)
{
	QStringList tokens = command.split(' ', QString::SkipEmptyParts);

	for(const OpCommand &c : COMMANDS.commands) {
		if(c.name() == tokens.at(0)) {
			if(c.checkParamCount(tokens.size()-1)) {
				try {
					c.call(client, command, tokens);
				} catch(const OpError &err) {
					client->sendSystemChat(command + ": " + err.message());
				}

			} else {
				client->sendSystemChat("Usage: /" + c.name() + " " + c.paramsDescription());
			}
			return;
		}
	}
	client->sendSystemChat("Unrecognized command: " + tokens.at(0));
}

}
