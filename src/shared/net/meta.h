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
	UserJoin(uint8_t ctx, const QByteArray &name) : Message(MSG_USER_JOIN, ctx), _name(name) {}
	UserJoin(uint8_t ctx, const QString &name) : UserJoin(ctx, name.toUtf8()) {}

	static UserJoin *deserialize(const uchar *data, uint len);

	QString name() const { return QString::fromUtf8(_name); }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	QByteArray _name;
};

class UserLeave : public Message {
public:
	explicit UserLeave(uint8_t ctx) : Message(MSG_USER_LEAVE, ctx) {}

	static UserLeave *deserialize(const uchar *data, uint len);

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;
};

class UserAttr: public Message {
public:
	static const uint8_t ATTR_LOCKED = 0x01;
	static const uint8_t ATTR_OP = 0x02;

	UserAttr(uint8_t ctx, uint8_t attrs) : Message(MSG_USER_ATTR, ctx), _attrs(attrs) {}
	UserAttr(uint8_t ctx, bool locked, bool op) : UserAttr(ctx, (locked?ATTR_LOCKED:0) | (op?ATTR_OP:0)) {}

	static UserAttr *deserialize(const uchar *data, uint len);

	uint8_t attrs() const { return _attrs; }

	bool isLocked() const { return _attrs & ATTR_LOCKED; }
	bool isOp() const { return _attrs & ATTR_OP; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint8_t _attrs;
};

class SessionTitle : public Message {
public:
	SessionTitle(uint8_t ctx, const QByteArray &title) : Message(MSG_SESSION_TITLE, ctx), _title(title) {}
	SessionTitle(uint8_t ctx, const QString &title) : SessionTitle(ctx, title.toUtf8()) {}

	static SessionTitle *deserialize(const uchar *data, uint len);

	QString title() const { return QString::fromUtf8(_title); }

	bool isOpCommand() const { return true; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	QByteArray _title;
};

class SessionConf : public Message {
public:
	SessionConf(bool locked, bool closed) : Message(MSG_SESSION_CONFIG, 0),
		_locked(locked), _closed(closed) {}

	static SessionConf *deserialize(const uchar *data, uint len);

	bool locked() const { return _locked; }
	bool closed() const { return _closed; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	bool _locked;
	bool _closed;
};

class Chat : public Message {
public:
	Chat(uint8_t ctx, const QByteArray &msg) : Message(MSG_CHAT, ctx), _msg(msg) {}
	Chat(uint8_t ctx, const QString &msg) : Chat(ctx, msg.toUtf8()) {}

	static Chat *deserialize(const uchar *data, uint len);

	QString message() const { return QString::fromUtf8(_msg); }

protected:
    int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
    QByteArray _msg;
};

class StreamPos : public Message {
public:
	StreamPos(uint32_t bytes) : Message(MSG_STREAMPOS, 0),
		_bytes(bytes) {}

	static StreamPos *deserialize(const uchar *data, uint len);

	/**
	 * @brief Number of bytes behind the current stream head
	 * @return byte count
	 */
	uint32_t bytes() const { return _bytes; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint32_t _bytes;
};

}

#endif
