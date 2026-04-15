// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_BRUSHES_LAZYBRUSH_H
#define LIBCLIENT_BRUSHES_LAZYBRUSH_H
#include "libclient/brushes/brush.h"
#include <QByteArray>

namespace brushes {

class LazyBrush final {
public:
	using LoaderFn = QByteArray (*)(void *, int);

	LazyBrush() = default;

	static LazyBrush fromLoader(LoaderFn fn, void *user);
	static LazyBrush fromBrush(const ActiveBrush &brush);
	static LazyBrush fromBrush(ActiveBrush &&brush);

	void clear();

	void setLoader(LoaderFn fn, void *user);
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
	void loadBrush(int presetId) const;

	mutable LoaderFn m_loaderFn = nullptr;
	void *m_loaderUser = nullptr;
	mutable ActiveBrush m_brush;
};

}

#endif
