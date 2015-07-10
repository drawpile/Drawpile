/*
   Drawpile - a collaborative drawing program.

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

#include "reader-compat.h"

#include "../net/message.h"
#include "../net/pen.h"
#include "../net/image.h"
#include "../net/layer.h"
#include "../net/meta.h"
#include "../net/annotation.h"

#include <QtEndian>

namespace recording {
namespace compat {

using namespace protocol;

// Pre 15.6 PutImage modes
uchar oldImageMode(uchar mode) {
	switch(mode) {
	case 0: return 255;
	case 1: return 1;
	case 2: return 11;
	case 3: return 0;
	}
	return 1;
}

LayerCreate *LayerCreateV14(const uchar *data, uint len)
{
	if(len<7)
		return 0;

	return new LayerCreate(
		*(data+0),
		qFromBigEndian<quint16>(data+1),
		0,
		qFromBigEndian<quint32>(data+3),
		0,
		QByteArray((const char*)data+7, len-7)
	);
}

LayerCreate *LayerCopyV14(const uchar *data, uint len)
{
	if(len<5)
		return 0;

	return new LayerCreate(
		*(data+0),
		qFromBigEndian<quint16>(data+1),
		qFromBigEndian<quint16>(data+3),
		0,
		LayerCreate::FLAG_COPY|LayerCreate::FLAG_INSERT,
		QByteArray((const char*)data+5, len-5)
	);
}

LayerCreate *LayerCreateV12(const uchar *data, uint len)
{
	if(len<6)
		return 0;

	return new LayerCreate(
		*(data+0),
		*(data+1),
		0,
		qFromBigEndian<quint32>(data+2),
		0,
		QByteArray((const char*)data+6, len-6)
	);
}

LayerAttributes *LayerAttributesV12(const uchar *data, uint len)
{
	if(len!=4)
		return 0;
	return new LayerAttributes(
		*(data+0),
		*(data+1),
		*(data+2),
		*(data+3)
	);
}

LayerRetitle *LayerRetitleV12(const uchar *data, uint len)
{
	if(len<2)
		return 0;
	return new LayerRetitle(
		*(data+0),
		*(data+1),
		QByteArray((const char*)data+2,len-2)
	);
}

LayerOrder *LayerOrderV12(const uchar *data, uint len)
{
	if(len<2 || len>256)
		return 0;

	QList<uint16_t> order;
	order.reserve(len);
	for(uint i=1;i<len;++i)
		order.append(data[i]);

	return new LayerOrder(data[0], order);
}

LayerDelete *LayerDeleteV12(const uchar *data, uint len)
{
	if(len != 3)
		return 0;
	return new LayerDelete(data[0], data[1], data[2]);
}


ToolChange *ToolChangeV11(const uchar *data, uint len)
{
	if(len != 19)
		return 0;

	return new ToolChange(
		*(data+0),
		*(data+1),
		*(data+2),
		*(data+3),
		*(data+4),
		qFromBigEndian<quint32>(data+5),
		qFromBigEndian<quint32>(data+9),
		*(data+13),
		*(data+14),
		qMax(1, *(data+15)*2), // brush size used to mean radius instead of diameter
		qMax(1, *(data+16)*2),
		*(data+17),
		*(data+18),
		0, // smudge parameters were added in V12
		0,
		0
	);
}

SessionConf *SessionConfV10(const uchar *data, uint len)
{
	if(len!=1)
			return 0;
	return new SessionConf(
		255, // no maxusers field in this version, but it doesn't really matter for playback
		data[0] | protocol::SessionConf::ATTR_PRESERVECHAT // this version had less flags and preserve chat was always on
	);
}

UserAttr *UserAttrV10(const uchar *data, uint len)
{
	if(len!=2)
			return 0;
	return new UserAttr(data[0], data[1]);
}

Chat *ChatV10(const uchar *data, uint len)
{
	if(len<2)
		return 0;
	// chat messages didn't have flags in this version
	return new Chat(*data, 0, QByteArray((const char*)data+1, len-1));
}

PutImage *PutImageV15_5(const uchar *data, uint len)
{
	if(len < 20)
		return 0;

	return new PutImage(
		*(data+0),
		qFromBigEndian<quint16>(data+1),
		oldImageMode(*(data+3)),
		qFromBigEndian<quint32>(data+4),
		qFromBigEndian<quint32>(data+8),
		qFromBigEndian<quint32>(data+12),
		qFromBigEndian<quint32>(data+16),
		QByteArray((const char*)data+20, len-20)
	);
}

PutImage *PutImageV10(const uchar *data, uint len)
{
	if(len < 11)
		return 0;

	return new PutImage(
		*(data+0),
		*(data+1),
		oldImageMode(*(data+2)),
		qFromBigEndian<quint16>(data+3),
		qFromBigEndian<quint16>(data+5),
		qFromBigEndian<quint16>(data+7),
		qFromBigEndian<quint16>(data+9),
		QByteArray((const char*)data+11, len-11)
	);
}

CanvasResize *CanvasResizeV10(const uchar *data, uint len)
{
	if(len!=9)
		return 0;

	return new CanvasResize(
		*data,
		qFromBigEndian<quint16>(data+1),
		qFromBigEndian<quint16>(data+3),
		qFromBigEndian<quint16>(data+5),
		qFromBigEndian<quint16>(data+7)
	);
}

FillRect *FillRectV10(const uchar *data, uint len)
{
	if(len != 15)
		return 0;

	return new FillRect(
		*(data+0),
		*(data+1),
		*(data+2),
		qFromBigEndian<quint16>(data+3),
		qFromBigEndian<quint16>(data+5),
		qFromBigEndian<quint16>(data+7),
		qFromBigEndian<quint16>(data+9),
		qFromBigEndian<quint32>(data+11)
	);
}

ToolChange *ToolChangeV12(const uchar *data, uint len)
{
	if(len != 22)
		return 0;

	return new ToolChange(
		*(data+0),
		*(data+1),
		*(data+2),
		*(data+3),
		*(data+4),
		qFromBigEndian<quint32>(data+5),
		qFromBigEndian<quint32>(data+9),
		*(data+13),
		*(data+14),
		*(data+15),
		*(data+16),
		*(data+17),
		*(data+18),
		*(data+19),
		*(data+20),
		*(data+21)
	);
}

PutImage *PutImageV12(const uchar *data, uint len)
{
	if(len < 19)
		return 0;

	return new PutImage(
		*(data+0),
		*(data+1),
		oldImageMode(*(data+2)),
		qFromBigEndian<quint32>(data+3),
		qFromBigEndian<quint32>(data+7),
		qFromBigEndian<quint32>(data+11),
		qFromBigEndian<quint32>(data+15),
		QByteArray((const char*)data+19, len-19)
	);
}

FillRect *FillRectV12(const uchar *data, uint len)
{
	if(len != 23)
		return 0;

	return new FillRect(
		*(data+0),
		*(data+1),
		*(data+2),
		qFromBigEndian<quint32>(data+3),
		qFromBigEndian<quint32>(data+7),
		qFromBigEndian<quint32>(data+11),
		qFromBigEndian<quint32>(data+15),
		qFromBigEndian<quint32>(data+19)
	);
}

AnnotationCreate *AnnotationCreateV12(const uchar *data, uint len)
{
	if(len!=14)
		return 0;
	return new AnnotationCreate(
		*(data+0),
		*(data+1),
		qFromBigEndian<qint32>(data+2),
		qFromBigEndian<qint32>(data+6),
		qFromBigEndian<quint16>(data+10),
		qFromBigEndian<quint16>(data+12)
	);
}

AnnotationReshape *AnnotationReshapeV12(const uchar *data, uint len)
{
	if(len!=14)
		return 0;
	return new AnnotationReshape(
		*(data+0),
		*(data+1),
		qFromBigEndian<qint32>(data+2),
		qFromBigEndian<qint32>(data+6),
		qFromBigEndian<quint16>(data+10),
		qFromBigEndian<quint16>(data+12)
	);
}

AnnotationEdit *AnnotationEditV12(const uchar *data, uint len)
{
	if(len < 6)
		return 0;

	return new AnnotationEdit(
		*data,
		*(data+1),
		qFromBigEndian<quint32>(data+2),
		QByteArray((const char*)data+6, len-6)
	);
}

AnnotationDelete *AnnotationDeleteV12(const uchar *data, uint len)
{
	if(len != 2)
		return 0;
	return new AnnotationDelete(data[0], data[1]);
}

Message *deserializeV15_5(const uchar *data, int length)
{
	if(length<Message::HEADER_LEN)
		return nullptr;

	const quint16 len = qFromBigEndian<quint16>(data);

	if(length < len+Message::HEADER_LEN)
		return nullptr;

	const MessageType type = MessageType(data[2]);

	switch(type) {
	case MSG_PUTIMAGE: return PutImageV15_5(data + Message::HEADER_LEN, len);
	default: return Message::deserialize(data, length);
	}
}

Message *deserializeV14(const uchar *data, int length)
{
	if(length<Message::HEADER_LEN)
		return nullptr;

	const quint16 len = qFromBigEndian<quint16>(data);

	if(length < len+Message::HEADER_LEN)
		return nullptr;

	const MessageType type = MessageType(data[2]);

	data += Message::HEADER_LEN;
	switch(type) {
	case MSG_LAYER_CREATE: return LayerCreateV14(data, len);
	case MSG_LAYER_COPY: return LayerCopyV14(data, len);
	default: return deserializeV15_5(data-Message::HEADER_LEN, length);
	}
}

Message *deserializeV12(const uchar *data, int length)
{
	if(length<Message::HEADER_LEN)
		return nullptr;

	const quint16 len = qFromBigEndian<quint16>(data);

	if(length < len+Message::HEADER_LEN)
		return nullptr;

	const MessageType type = MessageType(data[2]);

	data += Message::HEADER_LEN;
	switch(type) {
	case MSG_PUTIMAGE: return PutImageV12(data, len);
	case MSG_FILLRECT: return FillRectV12(data, len);
	case MSG_TOOLCHANGE: return ToolChangeV12(data, len);
	case MSG_LAYER_CREATE: return LayerCreateV12(data, len);
	case MSG_LAYER_ATTR: return LayerAttributesV12(data, len);
	case MSG_LAYER_RETITLE: return LayerRetitleV12(data, len);
	case MSG_LAYER_ORDER: return LayerOrderV12(data, len);
	case MSG_ANNOTATION_CREATE: return AnnotationCreateV12(data, len);
	case MSG_ANNOTATION_RESHAPE: return AnnotationReshapeV12(data, len);
	case MSG_ANNOTATION_EDIT: return AnnotationEditV12(data, len);
	case MSG_ANNOTATION_DELETE: return AnnotationDeleteV12(data, len);
	default: return deserializeV14(data-Message::HEADER_LEN, length);
	}
}

Message *deserializeV11(const uchar *data, int length)
{
	if(length<Message::HEADER_LEN)
		return nullptr;

	const quint16 len = qFromBigEndian<quint16>(data);

	if(length < len+Message::HEADER_LEN)
		return nullptr;

	const MessageType type = MessageType(data[2]);

	// ToolChange is the only (recordable) message type changed between V11 and V12
	if(type == MSG_TOOLCHANGE) {
		return ToolChangeV11(data + Message::HEADER_LEN, len);
	} else {
		return deserializeV12(data, length);
	}
}

Message *deserializeV10(const uchar *data, int length)
{
	if(length<Message::HEADER_LEN)
		return nullptr;

	const quint16 len = qFromBigEndian<quint16>(data);

	if(length < len+Message::HEADER_LEN)
		return nullptr;

	const MessageType type = MessageType(data[2]);

	switch(type) {
	case MSG_SESSION_CONFIG: return SessionConfV10(data + Message::HEADER_LEN, len);
	case MSG_USER_ATTR: return UserAttrV10(data + Message::HEADER_LEN, len);
	case MSG_CHAT: return ChatV10(data + Message::HEADER_LEN, len);
	case MSG_PUTIMAGE: return PutImageV10(data + Message::HEADER_LEN, len);
	case MSG_CANVAS_RESIZE: return CanvasResizeV10(data + Message::HEADER_LEN, len);
	case MSG_FILLRECT: return FillRectV10(data + Message::HEADER_LEN, len);
	default: return deserializeV11(data, length);
	}
}

}
}

