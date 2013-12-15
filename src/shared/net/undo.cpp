#include "undo.h"

namespace protocol {

UndoPoint *UndoPoint::deserialize(const uchar *data, uint len)
{
	if(len!=1)
		return 0;
	return new UndoPoint(*data);
}

int UndoPoint::payloadLength() const
{
	return 1;
}

int UndoPoint::serializePayload(uchar *data) const
{
	*data = contextId();
	return 1;
}


Undo *Undo::deserialize(const uchar *data, uint len)
{
	if(len!=3)
		return 0;
	return new Undo(
		data[0],
		data[1],
		data[2]
	);
}

int Undo::payloadLength() const
{
	return 1 + 2;
}

int Undo::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	*(ptr++) = _override;
	*(ptr++) = _points;
	return ptr-data;
}

}
