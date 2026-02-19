// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_EXPORT_VIDEOFORMAT_H
#define LIBCLIENT_EXPORT_VIDEOFORMAT_H

enum class VideoFormat {
	Frames,
	Zip,
	Gif,
	Webp,
	Mp4Vp9,
	WebmVp8,
	Mp4H264,
	Mp4Av1,
};

bool isVideoFormatSupported(VideoFormat format);

bool isVideoFormatSupportedFfmpeg(VideoFormat format);

#endif
