/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2017 Calle Laakkonen

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

#include "libshared/net/meta.h"
#include "libshared/net/textmode.h"

#include <QtEndian>
#include <cstring>

namespace protocol {

UserJoin *UserJoin::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len<2)
		return nullptr;

	const uint8_t flags = data[0];
	const uint nameLen = data[1];

	// Name must be at least one character long, but avatar is optional
	if(nameLen==0 || nameLen+2 > len)
		return nullptr;

	const QByteArray name = QByteArray(reinterpret_cast<const char*>(data)+2, nameLen);
	const QByteArray avatar = QByteArray(reinterpret_cast<const char*>(data)+2+nameLen, len-2-nameLen);
	return new UserJoin(ctx, flags, name, avatar);
}

int UserJoin::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = m_flags;
	*(ptr++) = m_name.length();
	memcpy(ptr, m_name.constData(), m_name.length());
	ptr += m_name.length();
	memcpy(ptr, m_avatar.constData(), m_avatar.length());
	ptr += m_avatar.length();

	return ptr - data;
}

int UserJoin::payloadLength() const
{
	return 1 + 1 + m_name.length() + m_avatar.length();
}

Kwargs UserJoin::kwargs() const
{
	Kwargs kw;
	kw["name"] = name();
	if(!m_avatar.isEmpty())
		kw["avatar"] = m_avatar.toBase64();
	QStringList flags;
	if(isModerator())
		flags << "mod";
	if(isAuthenticated())
		flags << "auth";
	if(!flags.isEmpty())
		kw["flags"] = flags.join(',');
	return kw;
}

UserJoin *UserJoin::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	QStringList flags = kwargs["flags"].split(',');

	return new UserJoin(
		ctx,
		(flags.contains("mod") ? FLAG_MOD : 0) |
		(flags.contains("auth") ? FLAG_AUTH : 0),
		kwargs["name"],
		QByteArray::fromBase64(kwargs["avatar"].toLatin1())
		);
}


SessionOwner *SessionOwner::deserialize(uint8_t ctx, const uchar *data, int len)
{
	if(len>255)
		return nullptr;

	QList<uint8_t> ids;
	ids.reserve(len);
	for(int i=0;i<len;++i)
		ids.append(data[i]);

	return new SessionOwner(ctx, ids);
}

int SessionOwner::serializePayload(uchar *data) const
{
	for(int i=0;i<m_ids.size();++i)
		data[i] = m_ids[i];
	return m_ids.size();
}

int SessionOwner::payloadLength() const
{
	return m_ids.size();
}

Kwargs SessionOwner::kwargs() const
{
	Kwargs kw;
	if(!m_ids.isEmpty())
		kw["users"] = text::idListString(m_ids);
	return kw;
}

SessionOwner *SessionOwner::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	return new SessionOwner(ctx, text::parseIdListString8(kwargs["users"]));
}


TrustedUsers *TrustedUsers::deserialize(uint8_t ctx, const uchar *data, int len)
{
	if(len>255)
		return nullptr;

	QList<uint8_t> ids;
	ids.reserve(len);
	for(int i=0;i<len;++i)
		ids.append(data[i]);

	return new TrustedUsers(ctx, ids);
}

int TrustedUsers::serializePayload(uchar *data) const
{
	for(int i=0;i<m_ids.size();++i)
		data[i] = m_ids[i];
	return m_ids.size();
}

int TrustedUsers::payloadLength() const
{
	return m_ids.size();
}

Kwargs TrustedUsers::kwargs() const
{
	Kwargs kw;
	if(!m_ids.isEmpty())
		kw["users"] = text::idListString(m_ids);
	return kw;
}

TrustedUsers *TrustedUsers::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	return new TrustedUsers(ctx, text::parseIdListString8(kwargs["users"]));
}

Chat *Chat::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len<3)
		return nullptr;
	return new Chat(ctx, *(data+0), *(data+1), QByteArray(reinterpret_cast<const char*>(data)+2, len-2));
}

int Chat::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = transparentFlags();
	*(ptr++) = opaqueFlags();
	memcpy(ptr, m_msg.constData(), m_msg.length());
	ptr += m_msg.length();
	return ptr - data;
}

int Chat::payloadLength() const
{
	return 2 + m_msg.length();
}

Kwargs Chat::kwargs() const
{
	Kwargs kw;
	kw["message"] = message();
	QStringList flags;
	if(isBypass())
		flags << "bypass";
	if(isShout())
		flags << "shout";
	if(isAction())
		flags << "action";
	if(isPin())
		flags << "pin";
	if(!flags.isEmpty())
		kw["flags"] = flags.join(',');
	return kw;
}

Chat *Chat::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	QStringList flags = kwargs["flags"].split(',');
	return new Chat(
		ctx,
		(flags.contains("bypass") ? FLAG_BYPASS : 0),
		(flags.contains("shout") ? FLAG_SHOUT : 0) |
		(flags.contains("action") ? FLAG_ACTION : 0) |
		(flags.contains("pin") ? FLAG_PIN : 0),
		kwargs["message"]
		);
}

PrivateChat *PrivateChat::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len<3)
		return nullptr;
	return new PrivateChat(ctx, *(data+0), *(data+1), QByteArray(reinterpret_cast<const char*>(data)+2, len-2));
}

int PrivateChat::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = target();
	*(ptr++) = opaqueFlags();
	memcpy(ptr, m_msg.constData(), m_msg.length());
	ptr += m_msg.length();
	return ptr - data;
}

int PrivateChat::payloadLength() const
{
	return 2 + m_msg.length();
}

Kwargs PrivateChat::kwargs() const
{
	Kwargs kw;
	kw["target"] = QString::number(target());
	kw["message"] = message();
	if(isAction()) kw["flags"] = "action";
	return kw;
}

PrivateChat *PrivateChat::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	QStringList flags = kwargs["flags"].split(',');
	return new PrivateChat(
		ctx,
		kwargs["target"].toInt(),
		(flags.contains("action") ? FLAG_ACTION : 0),
		kwargs["message"]
		);
}

}

