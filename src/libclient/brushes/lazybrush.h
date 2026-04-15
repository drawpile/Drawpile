// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_BRUSHES_LAZYBRUSH_H
#define LIBCLIENT_BRUSHES_LAZYBRUSH_H
#include "libclient/brushes/brush.h"
#include <QByteArray>

namespace brushes {

class LazyBrush {
public:
	LazyBrush() = default;

	static LazyBrush fromBytes(const QByteArray &bytes);
	static LazyBrush fromBytes(QByteArray &&bytes);
	static LazyBrush fromBrush(const ActiveBrush &brush);
	static LazyBrush fromBrush(ActiveBrush &&brush);

	void clear();

	void setBytes(const QByteArray &bytes);
	void setBytes(QByteArray &&bytes);
	void setBrush(const ActiveBrush &brush);
	void setBrush(ActiveBrush &&brush);

	ActiveBrush &brush(int presetId)
	{
		loadBrush(presetId);
		return m_brush;
	}

	const ActiveBrush &constBrush(int presetId) const
	{
		loadBrush(presetId);
		return m_brush;
	}

private:
	static constexpr int STATE_BLANK = 0;
	static constexpr int STATE_BYTES = 1;
	static constexpr int STATE_BRUSH = 2;

	void loadBrush(int presetId) const;

	mutable int m_state = STATE_BLANK;
	mutable QByteArray m_bytes;
	mutable ActiveBrush m_brush;
};

}

#endif
