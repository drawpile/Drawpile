// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_EXPORT_THUMBNAILERRUNNABLE_H
#define LIBCLIENT_EXPORT_THUMBNAILERRUNNABLE_H
#include "libclient/drawdance/canvasstate.h"
#include "libshared/net/message.h"
#include <QByteArray>
#include <QObject>
#include <QRunnable>

struct DP_Image;
struct DP_Output;

class ThumbnailerRunnable final : public QObject, public QRunnable {
	Q_OBJECT
public:
	enum class Format { Jpeg, Webp };

	ThumbnailerRunnable(
		unsigned int contextId, const QByteArray &correlator,
		const drawdance::CanvasState &canvasState, int maxWidth, int maxHeight,
		int quality, const QString &format);

	void run() override;

signals:
	void thumbnailGenerationFinished(const net::Message &msg);
	void thumbnailGenerationFailed(
		const QByteArray &correlator, const QString &error);

private:
	static constexpr int MAX_CORRELATOR_LENGTH = 100;
	static constexpr int MAX_DIMENSION = 1000;
	static constexpr int DEFAULT_DIMENSION = 400;
	static constexpr Format DEFAULT_FORMAT = Format::Webp;
	static constexpr int DEFAULT_QUALITY_JPEG = 80;
	static constexpr int DEFAULT_QUALITY_WEBP = 80;

	static int sanitizeDimension(int dimension);
	static Format sanitizeFormat(const QString &format);

	static bool writeCallback(void *user, DP_Image *img, DP_Output *output);
	bool write(DP_Image *img, DP_Output *output) const;
	bool writeJpeg(DP_Image *img, DP_Output *output, size_t maxSize) const;
	bool writeWebp(DP_Image *img, DP_Output *output, size_t maxSize) const;

	const QByteArray m_correlator;
	const drawdance::CanvasState m_canvasState;
	const unsigned int m_contextId;
	const int m_maxWidth;
	const int m_maxHeight;
	const int m_quality;
	const Format m_format;
};

#endif
