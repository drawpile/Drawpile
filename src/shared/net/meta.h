/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/
#ifndef DP_NET_META_H
#define DP_NET_META_H

#include <QString>

#include "message.h"

namespace protocol {

class UserJoin : public Message {
public:
	UserJoin(uint8_t id, const QByteArray &name) : Message(MSG_USER_JOIN), _id(id), _name(name) {}
	UserJoin(uint8_t id, const QString &name) : UserJoin(id, name.toUtf8()) {}

	static UserJoin *deserialize(const uchar *data, uint len);

	uint8_t id() const { return _id; }
	QString name() const { return QString::fromUtf8(_name); }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint8_t _id;
	QByteArray _name;
};

class UserLeave : public Message {
public:
	explicit UserLeave(uint8_t id) : Message(MSG_USER_LEAVE), _id(id) {}

	static UserLeave *deserialize(const uchar *data, uint len);

	uint8_t id() const { return _id; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint8_t _id;
};

class UserAttr: public Message {
public:
	static const uint8_t ATTR_LOCKED = 0x01;
	static const uint8_t ATTR_OP = 0x02;

	UserAttr(uint8_t id, uint8_t attrs) : Message(MSG_USER_ATTR), _id(id), _attrs(attrs) {}
	UserAttr(uint8_t id, bool locked, bool op) : UserAttr(id, (locked?ATTR_LOCKED:0) | (op?ATTR_OP:0)) {}

	static UserAttr *deserialize(const uchar *data, uint len);

	uint8_t id() const { return _id; }
	uint8_t attrs() const { return _attrs; }

	bool isLocked() const { return _attrs & ATTR_LOCKED; }
	bool isOp() const { return _attrs & ATTR_OP; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint8_t _id;
	uint8_t _attrs;
};

class SessionTitle : public Message {
public:
	SessionTitle(const QByteArray &title) : Message(MSG_SESSION_TITLE), _title(title) {}
	SessionTitle(const QString &title) : SessionTitle(title.toUtf8()) {}

	static SessionTitle *deserialize(const uchar *data, uint len);

	QString title() const { return QString::fromUtf8(_title); }

	bool isOpCommand() const { return true; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	QByteArray _title;
};

class Chat : public Message {
public:
	Chat(uint8_t user, const QByteArray &msg) : Message(MSG_CHAT), _user(user), _msg(msg) {}
	Chat(uint8_t user, const QString &msg) : Chat(user, msg.toUtf8()) {}

	static Chat *deserialize(const uchar *data, uint len);

	uint8_t user() const { return _user; }
	QString message() const { return QString::fromUtf8(_msg); }

	void setOrigin(uint8_t userid) { _user = userid; }

protected:
    int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint8_t _user;
    QByteArray _msg;
};

}

#endif
