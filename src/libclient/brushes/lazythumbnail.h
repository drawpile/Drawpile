// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_BRUSHES_LAZYTHUMBNAIL_H
#define LIBCLIENT_BRUSHES_LAZYTHUMBNAIL_H
#include <QAtomicInt>
#include <QByteArray>
#include <QPixmap>

namespace brushes {

class LazyThumbnail {
public:
	LazyThumbnail() = default;

	static LazyThumbnail fromPixmap(const QPixmap &pixmap);

	void clear();

	// These clear any current contents and replace the single parameter.
	// If the given parameter is empty or null respectively, just clears.
	void setBytes(const QByteArray &bytes);
	void setPixmap(const QPixmap &pixmap);

	const QByteArray &bytes() const;
	const QPixmap &pixmap(int presetId) const;

	static QByteArray toPng(const QPixmap &pixmap);

private:
	static constexpr int HAVE_BYTES = 1 << 0;
	static constexpr int HAVE_PIXMAP = 1 << 1;
	static constexpr int HAVE_BOTH = HAVE_BYTES | HAVE_PIXMAP;

	mutable int m_flags;
	mutable QByteArray m_bytes;
	mutable QPixmap m_pixmap;
};

}

#endif
