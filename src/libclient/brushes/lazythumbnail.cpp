// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/brushes/lazythumbnail.h"
#include <QBuffer>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(
	lcDpLazyThumbnail, "net.drawpile.brushes.lazythumbnail", QtWarningMsg)

namespace brushes {

LazyThumbnail LazyThumbnail::fromPixmap(const QPixmap &pixmap)
{
	LazyThumbnail lt;
	lt.setPixmap(pixmap);
	return lt;
}

void LazyThumbnail::clear()
{
	m_flags = 0;
	m_bytes.clear();
	m_pixmap = QPixmap();
}

void LazyThumbnail::setBytes(const QByteArray &bytes)
{
	if(bytes.isEmpty()) {
		clear();
	} else {
		m_flags = HAVE_BYTES;
		m_bytes = bytes;
		m_pixmap = QPixmap();
	}
}

void LazyThumbnail::setPixmap(const QPixmap &pixmap)
{
	if(pixmap.isNull()) {
		clear();
	} else {
		m_flags = HAVE_PIXMAP;
		m_bytes.clear();
		m_pixmap = pixmap;
	}
}

const QByteArray &LazyThumbnail::bytes() const
{
	if(m_flags == HAVE_PIXMAP) {
		m_flags |= HAVE_BYTES;
		m_bytes = toPng(m_pixmap);
	}
	return m_bytes;
}

const QPixmap &LazyThumbnail::pixmap(int presetId) const
{
	if(m_flags == HAVE_BYTES) {
		m_flags |= HAVE_PIXMAP;
		if(!m_bytes.isEmpty()) {
			qCDebug(
				lcDpLazyThumbnail, "Loading thumbnail for preset %d", presetId);
			if(!m_pixmap.loadFromData(m_bytes)) {
				qCWarning(
					lcDpLazyThumbnail, "Failed to load thumbnail for preset %d",
					presetId);
			}
		}
	}
	return m_pixmap;
}

QByteArray LazyThumbnail::toPng(const QPixmap &pixmap)
{
	QByteArray bytes;
	if(!pixmap.isNull()) {
		QBuffer buffer(&bytes);
		buffer.open(QIODevice::WriteOnly);
		pixmap.save(&buffer, "PNG");
		buffer.close();
	}
	return bytes;
}

}
