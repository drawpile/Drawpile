// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_EXPORT_VIDEOFORMAT_H
#define LIBCLIENT_EXPORT_VIDEOFORMAT_H
#include <QString>
#include <QVector>

enum class VideoFormat {
	Frames,
	Zip,
	Gif,
	Webp,
	Mp4Vp9,
	WebmVp8,
	Mp4H264,
	Mp4Av1,
	Apng,
	Last = Apng,
};

enum class VideoFormatApplication {
	Animation,
	Timelapse,
};

struct VideoFormatOption {
	VideoFormat format;
	QString title;
	bool libavSupported;
	bool ffmpegSupported;
};

bool isVideoFormatSupported(VideoFormat format);

bool isVideoFormatSupportedFfmpeg(VideoFormat format);

QVector<VideoFormatOption> getVideoFormatOptions(
	VideoFormatApplication application, bool *outAnyFfmpegSupported);

#endif
