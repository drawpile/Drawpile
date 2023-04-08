// SPDX-License-Identifier: GPL-3.0-or-later

#include "libshared/net/undo.h"

namespace protocol {

Undo *Undo::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len!=2)
		return nullptr;
	return new Undo(
		ctx,
		data[0],
		data[1]
	);
}

int Undo::payloadLength() const
{
	return 2;
}

int Undo::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = m_override;
	*(ptr++) = m_redo;
	return ptr-data;
}

bool Undo::payloadEquals(const Message &m) const
{
	const Undo &u = static_cast<const Undo&>(m);
	return
		overrideId() == u.overrideId() &&
		isRedo() == u.isRedo();
}

Kwargs Undo::kwargs() const
{
	Kwargs kw;
	if(overrideId()>0)
		kw["override"] = QString::number(overrideId());
	return kw;
}

Undo *Undo::fromText(uint8_t ctx, const Kwargs &kwargs, bool redo)
{
	return new Undo(
		ctx,
		kwargs["override"].toInt(),
		redo
		);
}

}
