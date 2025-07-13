// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/output.h>
#include <dpengine/image.h>
#include <dpimpex/image_impex.h>
}
#include "libclient/drawdance/global.h"
#include "libclient/export/thumbnailerrunnable.h"
#include "libshared/util/qtcompat.h"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcDpThumbnailer, "net.drawpile.thumbnailer", QtWarningMsg)

ThumbnailerRunnable::ThumbnailerRunnable(
	unsigned int contextId, const QByteArray &correlator,
	const drawdance::CanvasState &canvasState, int maxWidth, int maxHeight,
	int quality, const QString &format)
	: m_correlator(correlator)
	, m_canvasState(canvasState)
	, m_contextId(contextId)
	, m_maxWidth(sanitizeDimension(maxWidth))
	, m_maxHeight(sanitizeDimension(maxHeight))
	, m_quality(quality)
	, m_format(sanitizeFormat(format))
{
}

void ThumbnailerRunnable::run()
{
	if(m_correlator.size() > MAX_CORRELATOR_LENGTH) {
		qCWarning(
			lcDpThumbnailer, "Thumbnail generation correlator is too long");
		emit thumbnailGenerationFailed(
			m_correlator, QStringLiteral("badcorrelator"));
		return;
	}

	void *buffer;
	size_t size;
	bool ok;
	{
		DP_DrawContext *dc;
#if defined(Q_OS_ANDROID) || defined(__EMSCRIPTEN__)
		dc = nullptr;
#else
		drawdance::DrawContext drawContext =
			drawdance::DrawContextPool::acquire();
		dc = drawContext.get();
#endif
		ok = DP_image_thumbnail_from_canvas_write(
			m_canvasState.get(), dc, m_maxWidth, m_maxHeight,
			&ThumbnailerRunnable::writeCallback, this, &buffer, &size);
	}

	if(!ok) {
		qCWarning(
			lcDpThumbnailer, "Thumbnail generation failed: %s", DP_error());
		emit thumbnailGenerationFailed(m_correlator, QStringLiteral("genfail"));
		return;
	}

	net::Message msg = net::makeThumbnailMessage(
		m_contextId, m_correlator,
		QByteArray::fromRawData(
			reinterpret_cast<const char *>(buffer), compat::castSize(size)));
	DP_free(buffer);
	if(msg.isNull()) {
		// Should really be caught in the write functions below.
		qCWarning(
			lcDpThumbnailer,
			"Generated thumbnail is too large (%zu bytes + %zu bytes of "
			"correlator exceeds maximum of %zu bytes)",
			size_t(size), size_t(m_correlator.size()),
			size_t(DP_MSG_THUMBNAIL_DATA_MAX_SIZE));
		emit thumbnailGenerationFailed(m_correlator, QStringLiteral("toobig"));
		return;
	}

	emit thumbnailGenerationFinished(msg);
}

int ThumbnailerRunnable::sanitizeDimension(int dimension)
{
	if(dimension <= 0) {
		return DEFAULT_DIMENSION;
	} else {
		return qMin(MAX_DIMENSION, dimension);
	}
}

ThumbnailerRunnable::Format
ThumbnailerRunnable::sanitizeFormat(const QString &format)
{
	if(format.isEmpty()) {
		return DEFAULT_FORMAT;
	} else if(
		QStringLiteral("jpg").compare(format, Qt::CaseInsensitive) == 0 ||
		QStringLiteral("jpeg").compare(format, Qt::CaseInsensitive) == 0) {
		return Format::Jpeg;
	} else if(
		QStringLiteral("webp").compare(format, Qt::CaseInsensitive) == 0) {
		return Format::Webp;
	} else {
		qCWarning(
			lcDpThumbnailer,
			"Unknown thumbnail format '%s' requested, defaulting to WEBP",
			qUtf8Printable(format));
		return Format::Jpeg;
	}
}

bool ThumbnailerRunnable::writeCallback(
	void *user, DP_Image *img, DP_Output *output)
{
	return static_cast<ThumbnailerRunnable *>(user)->write(img, output);
}

bool ThumbnailerRunnable::write(DP_Image *img, DP_Output *output) const
{
	size_t maxSize =
		size_t(DP_MSG_THUMBNAIL_DATA_MAX_SIZE) - size_t(m_correlator.size());
	switch(m_format) {
	case Format::Jpeg:
		return writeJpeg(img, output, maxSize);
	case Format::Webp:
		return writeWebp(img, output, maxSize);
	default:
		DP_error_set("Unknown thumbnail format %d", int(m_format));
		return false;
	}
}

bool ThumbnailerRunnable::writeJpeg(
	DP_Image *img, DP_Output *output, size_t maxSize) const
{
	int quality = m_quality <= 0 ? DEFAULT_QUALITY_JPEG : qMin(100, m_quality);
	while(true) {
		bool ok = DP_image_write_jpeg_quality(img, output, quality);
		if(!ok) {
			return false;
		}

		bool error;
		size_t size = DP_output_tell(output, &error);
		if(error) {
			return false;
		}

		if(size < maxSize) {
			qCDebug(
				lcDpThumbnailer,
				"Generated %dx%d JPEG at quality %d size %zu/%zu",
				DP_image_width(img), DP_image_height(img), quality, size,
				maxSize);
			return true;
		}

		qCDebug(
			lcDpThumbnailer, "%dx%d JPEG at quality %d size %zu exceeds %zu",
			DP_image_width(img), DP_image_height(img), quality, size, maxSize);
		if(quality == 1) {
			DP_error_set(
				"Could not generate JPEG thumbnail <= %zu bytes", maxSize);
			return false;
		}

		if(!DP_output_clear(output)) {
			return false;
		}

		quality = DP_max_int(1, DP_min_int(quality - 10, quality / 2));
	}
}

bool ThumbnailerRunnable::writeWebp(
	DP_Image *img, DP_Output *output, size_t maxSize) const
{
	int quality = m_quality <= 0 ? DEFAULT_QUALITY_WEBP : m_quality;
	while(true) {
		bool ok = quality > 100
					  ? DP_image_write_webp(img, output)
					  : DP_image_write_webp_lossy(img, output, quality);
		if(!ok) {
			return false;
		}

		bool error;
		size_t size = DP_output_tell(output, &error);
		if(error) {
			return false;
		}

		if(size < maxSize) {
			qCDebug(
				lcDpThumbnailer,
				"Generated %dx%d WEBP at quality %d size %zu/%zu",
				DP_image_width(img), DP_image_height(img), quality, size,
				maxSize);
			return true;
		}

		qCDebug(
			lcDpThumbnailer, "%dx%d WEBP at quality %d size %zu exceeds %zu",
			DP_image_width(img), DP_image_height(img), quality, size, maxSize);
		if(quality == 1) {
			DP_error_set(
				"Could not generate WEBP thumbnail <= %zu bytes", maxSize);
			return false;
		}

		if(!DP_output_clear(output)) {
			return false;
		}

		quality = quality > 100
					  ? 100
					  : DP_max_int(1, DP_min_int(quality - 10, quality / 2));
	}
}
