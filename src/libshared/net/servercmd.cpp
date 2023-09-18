// SPDX-License-Identifier: GPL-3.0-or-later
#include "libshared/net/servercmd.h"
#include "libshared/util/qtcompat.h"

namespace net {

net::Message ServerCommand::make(
	const QString &cmd, const QJsonArray &args, const QJsonObject &kwargs)
{
	return ServerCommand{cmd, args, kwargs}.toMessage();
}

net::Message ServerCommand::makeKick(int target, bool ban)
{
	Q_ASSERT(target > 0 && target < 256);
	QJsonObject kwargs;
	if(ban)
		kwargs["ban"] = true;

	return make("kick-user", QJsonArray() << target, kwargs);
}

net::Message ServerCommand::makeUnban(int entryId)
{
	return make("remove-ban", QJsonArray() << entryId);
}

net::Message ServerCommand::makeMute(int target, bool mute)
{
	return make("mute", QJsonArray() << target << mute);
}

net::Message ServerCommand::makeAnnounce(const QString &url, bool privateMode)
{
	QJsonObject kwargs;
	if(privateMode)
		kwargs["private"] = true;

	return make("announce-session", QJsonArray() << url, kwargs);
}

net::Message ServerCommand::makeUnannounce(const QString &url)
{
	return make("unlist-session", QJsonArray() << url);
}

net::Message ServerCommand::toMessage() const
{
	QJsonObject data{{QStringLiteral("cmd"), cmd}};
	if(!args.isEmpty()) {
		data["args"] = args;
	}
	if(!kwargs.isEmpty()) {
		data["kwargs"] = kwargs;
	}
	return net::makeServerCommandMessage(0, QJsonDocument{data});
}

ServerCommand ServerCommand::fromMessage(const net::Message &msg)
{
	if(msg.isNull() || msg.type() != DP_MSG_SERVER_COMMAND) {
		qWarning("ServerCommand::fromMessage: bad message");
		return ServerCommand{QString(), QJsonArray(), QJsonObject()};
	}

	size_t len;
	const char *data = DP_msg_server_command_msg(msg.toServerCommand(), &len);
	QByteArray bytes = QByteArray::fromRawData(data, compat::castSize(len));

	QJsonParseError err;
	const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
	if(err.error != QJsonParseError::NoError) {
		qWarning(
			"ServerReply::fromMessage JSON parsing error: %s",
			qPrintable(err.errorString()));
		return ServerCommand{QString(), QJsonArray(), QJsonObject()};
	}

	return fromJson(doc);
}

ServerCommand ServerCommand::fromJson(const QJsonDocument &doc)
{
	QJsonObject data = doc.object();
	return ServerCommand{
		data["cmd"].toString(), data["args"].toArray(),
		data["kwargs"].toObject()};
}


ServerReply ServerReply::fromMessage(const net::Message &msg)
{
	if(msg.isNull() || msg.type() != DP_MSG_SERVER_COMMAND) {
		qWarning("ServerReply::fromMessage: bad message");
		return ServerReply{
			ServerReply::ReplyType::Unknown, QString(), QJsonObject()};
	}

	size_t len;
	const char *data = DP_msg_server_command_msg(msg.toServerCommand(), &len);
	QByteArray bytes = QByteArray::fromRawData(data, compat::castSize(len));

	QJsonParseError err;
	const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
	if(err.error != QJsonParseError::NoError) {
		qWarning(
			"ServerReply::fromMessage JSON parsing error: %s",
			qPrintable(err.errorString()));
		return ServerReply{
			ServerReply::ReplyType::Unknown, QString(), QJsonObject()};
	}

	return fromJson(doc);
}

ServerReply ServerReply::fromJson(const QJsonDocument &doc)
{
	QJsonObject data = doc.object();
	QString typestr = data.value("type").toString();

	ServerReply r;
	if(typestr == "login")
		r.type = ServerReply::ReplyType::Login;
	else if(typestr == "msg")
		r.type = ServerReply::ReplyType::Message;
	else if(typestr == "alert")
		r.type = ServerReply::ReplyType::Alert;
	else if(typestr == "error")
		r.type = ServerReply::ReplyType::Error;
	else if(typestr == "result")
		r.type = ServerReply::ReplyType::Result;
	else if(typestr == "log")
		r.type = ServerReply::ReplyType::Log;
	else if(typestr == "sessionconf")
		r.type = ServerReply::ReplyType::SessionConf;
	else if(typestr == "sizelimit")
		r.type = ServerReply::ReplyType::SizeLimitWarning;
	else if(typestr == "status")
		r.type = ServerReply::ReplyType::Status;
	else if(typestr == "reset")
		r.type = ServerReply::ReplyType::Reset;
	else if(typestr == "autoreset")
		r.type = ServerReply::ReplyType::ResetRequest;
	else if(typestr == "catchup")
		r.type = ServerReply::ReplyType::Catchup;
	else
		r.type = ServerReply::ReplyType::Unknown;

	r.message = data.value("message").toString();
	r.reply = data;
	return r;
}

net::Message ServerReply::make(const QJsonObject &data)
{
	return net::makeServerCommandMessage(0, QJsonDocument{data});
}

net::Message ServerReply::makeError(const QString &message, const QString &code)
{
	return make(
		{{QStringLiteral("type"), QStringLiteral("error")},
		 {QStringLiteral("message"), message},
		 {QStringLiteral("code"), code}});
}

net::Message
ServerReply::makeCommandError(const QString &command, const QString &message)
{
	return make(
		{{QStringLiteral("type"), QStringLiteral("error")},
		 {QStringLiteral("message"),
		  QStringLiteral("%1: %2").arg(command, message)}});
}

net::Message ServerReply::makeMessage(const QString &message)
{
	return make(
		{{QStringLiteral("type"), QStringLiteral("message")},
		 {QStringLiteral("message"), message}});
}

net::Message ServerReply::makeAlert(const QString &message)
{
	return make(
		{{QStringLiteral("type"), QStringLiteral("alert")},
		 {QStringLiteral("message"), message}});
}

net::Message ServerReply::makeCatchup(int count)
{
	return make(
		{{QStringLiteral("type"), QStringLiteral("catchup")},
		 {QStringLiteral("count"), count}});
}

net::Message ServerReply::makeLog(const QString &message, QJsonObject data)
{
	data[QStringLiteral("type")] = QStringLiteral("log");
	data[QStringLiteral("message")] = message;
	return make(data);
}

net::Message ServerReply::makeLoginGreeting(
	const QString &message, int version, const QJsonArray &flags)
{
	return make(
		{{QStringLiteral("type"), QStringLiteral("login")},
		 {QStringLiteral("message"), message},
		 {QStringLiteral("version"), version},
		 {QStringLiteral("flags"), flags}});
}

net::Message ServerReply::makeLoginWelcome(
	const QString &message, const QString &title, const QJsonArray &sessions)
{
	return make(
		{{QStringLiteral("type"), QStringLiteral("login")},
		 {QStringLiteral("message"), message},
		 {QStringLiteral("title"), title},
		 {QStringLiteral("sessions"), sessions}});
}

net::Message
ServerReply::makeLoginTitle(const QString &message, const QString &title)
{
	return make(
		{{QStringLiteral("type"), QStringLiteral("login")},
		 {QStringLiteral("message"), message},
		 {QStringLiteral("title"), title}});
}

net::Message ServerReply::makeLoginSessions(
	const QString &message, const QJsonArray &sessions)
{
	return make(
		{{QStringLiteral("type"), QStringLiteral("login")},
		 {QStringLiteral("message"), message},
		 {QStringLiteral("sessions"), sessions}});
}

net::Message ServerReply::makeLoginRemoveSessions(
	const QString &message, const QJsonArray &remove)
{
	return make(
		{{QStringLiteral("type"), QStringLiteral("login")},
		 {QStringLiteral("message"), message},
		 {QStringLiteral("remove"), remove}});
}

net::Message
ServerReply::makeReset(const QString &message, const QString &state)
{
	return make(
		{{QStringLiteral("type"), QStringLiteral("reset")},
		 {QStringLiteral("message"), message},
		 {QStringLiteral("state"), state}});
}

net::Message ServerReply::makeResetRequest(int maxSize, bool query)
{
	return make(
		{{QStringLiteral("type"), QStringLiteral("autoreset")},
		 {QStringLiteral("maxSize"), maxSize},
		 {QStringLiteral("query"), query}});
}

net::Message ServerReply::makeResultPasswordNeeded(
	const QString &message, const QString &state)
{
	return make(
		{{QStringLiteral("type"), QStringLiteral("result")},
		 {QStringLiteral("message"), message},
		 {QStringLiteral("state"), state}});
}

net::Message ServerReply::makeResultLoginOk(
	const QString &message, const QString &state, const QJsonArray &flags,
	const QString &ident, bool guest)
{
	return make(
		{{QStringLiteral("type"), QStringLiteral("result")},
		 {QStringLiteral("message"), message},
		 {QStringLiteral("state"), state},
		 {QStringLiteral("flags"), flags},
		 {QStringLiteral("ident"), ident},
		 {QStringLiteral("guest"), guest}});
}

net::Message ServerReply::makeResultExtAuthNeeded(
	const QString &message, const QString &state, const QString &extauthurl,
	const QString &nonce, const QString &group, bool avatar)
{
	return make(
		{{QStringLiteral("type"), QStringLiteral("result")},
		 {QStringLiteral("message"), message},
		 {QStringLiteral("state"), state},
		 {QStringLiteral("extauthurl"), extauthurl},
		 {QStringLiteral("nonce"), nonce},
		 {QStringLiteral("group"), group},
		 {QStringLiteral("avatar"), avatar}});
}

net::Message ServerReply::makeResultJoinHost(
	const QString &message, const QString &state, const QJsonObject &join)
{
	return make(
		{{QStringLiteral("type"), QStringLiteral("result")},
		 {QStringLiteral("message"), message},
		 {QStringLiteral("state"), state},
		 {QStringLiteral("join"), join}});
}

net::Message
ServerReply::makeResultStartTls(const QString &message, bool startTls)
{
	return make(
		{{QStringLiteral("type"), QStringLiteral("result")},
		 {QStringLiteral("message"), message},
		 {QStringLiteral("startTls"), startTls}});
}

net::Message ServerReply::makeSessionConf(const QJsonObject &config)
{
	return make(
		{{QStringLiteral("type"), QStringLiteral("sessionconf")},
		 {QStringLiteral("config"), config}});
}

net::Message ServerReply::makeSizeLimitWarning(int size, int maxSize)
{
	return make(
		{{QStringLiteral("type"), QStringLiteral("sizelimit")},
		 {QStringLiteral("size"), size},
		 {QStringLiteral("maxSize"), maxSize}});
}

net::Message ServerReply::makeStatusUpdate(int size)
{
	return make(
		{{QStringLiteral("type"), QStringLiteral("status")},
		 {QStringLiteral("size"), size}});
}

}
