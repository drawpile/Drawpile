/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2021 Calle Laakkonen

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

#include "servercmd.h"
#include "envelopebuilder.h"
#include "../rustpile/rustpile.h"

namespace net {

Envelope ServerCommand::make(const QString &cmd, const QJsonArray &args, const QJsonObject &kwargs)
{
	return ServerCommand { cmd, args, kwargs }.toEnvelope();
}

Envelope ServerCommand::makeKick(int target, bool ban)
{
	Q_ASSERT(target>0 && target<256);
	QJsonObject kwargs;
	if(ban)
		kwargs["ban"] = true;

	return make("kick-user", QJsonArray() << target, kwargs);
}

Envelope ServerCommand::makeAnnounce(const QString &url, bool privateMode)
{
	QJsonObject kwargs;
	if(privateMode)
		kwargs["private"] = true;

	return make("announce-session", QJsonArray() << url, kwargs);
}

Envelope ServerCommand::makeUnannounce(const QString &url)
{
	return make("unlist-session", QJsonArray() << url);
}

Envelope ServerCommand::makeUnban(int entryId)
{
	return make("remove-ban", QJsonArray() << entryId);
}

Envelope ServerCommand::makeMute(int target, bool mute)
{
	return make("mute", QJsonArray() << target << mute);
}

Envelope ServerCommand::toEnvelope() const
{
	QJsonObject o;
	o["cmd"] = cmd;
	if(!args.isEmpty())
		o["args"] = args;
	if(!kwargs.isEmpty())
		o["kwargs"] = kwargs;

	const QByteArray payload = QJsonDocument(o).toJson(QJsonDocument::Compact);

	// TODO we should have a message type for splitting up overlong messages
	if(payload.length() > 0xffff - Envelope::HEADER_LEN) {
		qWarning(
			"ServerCommand::toEnvelope(%s) produced a message that is too long! (%lld bytes)",
			qPrintable(cmd),
			static_cast<long long>(payload.length())
		);
		return Envelope();
	}

	EnvelopeBuilder eb;
	rustpile::write_servercommand(eb, 0, reinterpret_cast<const uchar*>(payload.constData()), payload.length());

	return eb.toEnvelope();
}

static ServerReply ServerReplyFromJson(const QJsonDocument &doc)
{
	const QJsonObject o = doc.object();
	const QString typestr = o.value("type").toString();

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

	r.message = o.value("message").toString();
	r.reply = o;
	return r;
}

ServerReply ServerReply::fromEnvelope(const Envelope &e)
{
	const int len = e.messageLength();
	if(len == 0 || e.messageType() != 0) {
		qWarning("ServerReply::fromEnvelope: bad message. Type %d, length %d", e.messageType(), len);
		return ServerReply{
			ServerReply::ReplyType::Unknown,
			QString(),
			QJsonObject()
		};
	}

	const QByteArray payload = QByteArray::fromRawData(reinterpret_cast<const char*>(e.data() + Envelope::HEADER_LEN), len - Envelope::HEADER_LEN);

	QJsonParseError err;
	const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
	if(err.error != QJsonParseError::NoError) {
		qWarning("ServerReply::fromEnvelope JSON parsing error: %s", qPrintable(err.errorString()));
		return ServerReply{
			ServerReply::ReplyType::Unknown,
			QString(),
			QJsonObject()
		};
	}

	return ServerReplyFromJson(doc);
}

}
