/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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

#include "control.h"

#include <cstring>

#include <QtEndian>
#include <QDebug>

namespace protocol {

ServerCommand ServerCommand::fromJson(const QJsonDocument &doc)
{
	const QJsonObject o = doc.object();
	ServerCommand c;
	c.cmd = o.value("cmd").toString();
	c.args = o.value("args").toArray();
	c.kwargs = o.value("kwargs").toObject();
	return c;
}

QJsonDocument ServerCommand::toJson() const
{
	QJsonObject o;
	o["cmd"] = cmd;
	if(!args.isEmpty())
		o["args"] = args;
	if(!kwargs.isEmpty())
		o["kwargs"] = kwargs;
	return QJsonDocument(o);
}

ServerReply ServerReply::fromJson(const QJsonDocument &doc)
{
	const QJsonObject o = doc.object();
	ServerReply r;
	QString typestr = o.value("type").toString();

	if(typestr == "login")
		r.type = LOGIN;
	else if(typestr == "message")
		r.type = MESSAGE;
	else if(typestr == "alert")
		r.type = ALERT;
	else if(typestr == "error")
		r.type = ERROR;
	else if(typestr == "result")
		r.type = RESULT;
	else if(typestr == "sessionconf")
		r.type = SESSIONCONF;
	else if(typestr == "sizelimit")
		r.type = SIZELIMITWARNING;
	else if(typestr == "reset")
		r.type = RESET;
	else
		r.type = UNKNOWN;

	r.message = o.value("message").toString();
	r.reply = o;
	return r;
}

QJsonDocument ServerReply::toJson() const
{
	QJsonObject o = reply;
	QString typestr;
	switch(type) {
	case UNKNOWN: break;
	case LOGIN: typestr="login"; break;
	case MESSAGE: typestr="msg"; break;
	case ALERT: typestr="alert"; break;
	case ERROR: typestr="error"; break;
	case RESULT: typestr="result"; break;
	case SESSIONCONF: typestr="sessionconf"; break;
	case SIZELIMITWARNING: typestr="sizelimit"; break;
	case RESET: typestr="reset"; break;
	}
	o["type"] = typestr;

	if(!o.contains("message"))
		o["message"] = message;
	return QJsonDocument(o);
}

MessagePtr Command::error(const QString &message)
{
	ServerReply sr;
	sr.type = ServerReply::ERROR;
	sr.message = message;
	return MessagePtr(new Command(0, sr));
}

Command *Command::deserialize(uint8_t ctxid, const uchar *data, uint len)
{
	return new Command(ctxid, QByteArray((const char*)data, len));
}

int Command::serializePayload(uchar *data) const
{
	memcpy(data, m_msg.constData(), m_msg.length());
	return m_msg.length();
}

int Command::payloadLength() const
{
	return m_msg.length();
}

QJsonDocument Command::doc() const
{
	QJsonParseError e;
	QJsonDocument d = QJsonDocument::fromJson(m_msg, &e);
	if(e.error != QJsonParseError::NoError) {
		qWarning() << "JSON parse error:" << e.errorString();
	}
	return d;
}

Disconnect *Disconnect::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len<1)
		return 0;
	return new Disconnect(ctx, Reason(*data), QByteArray((const char*)data+1, len-1));
}

int Disconnect::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = _reason;
	memcpy(ptr, _message.constData(), _message.length());
	ptr += _message.length();
	return ptr - data;
}

int Disconnect::payloadLength() const
{
	return 1 + _message.length();
}

StreamPos *StreamPos::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len!=4)
		return 0;
	return new StreamPos(ctx, qFromBigEndian<quint32>(data));
}

int StreamPos::serializePayload(uchar *data) const
{
	qToBigEndian(_bytes, data);
	return 4;
}

int StreamPos::payloadLength() const
{
	return 4;
}

Ping *Ping::deserialize(uint8_t ctx, const uchar *data, int len)
{
	if(len!=1)
		return nullptr;
	return new Ping(ctx, *data);
}

int Ping::payloadLength() const
{
	return 1;
}

int Ping::serializePayload(uchar *data) const
{
	*data = _isPong;
	return 1;
}

}
