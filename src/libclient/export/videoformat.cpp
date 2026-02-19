// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/export/videoformat.h"
#include <QtGlobal>
#ifdef DP_LIBAV
extern "C" {
#	include <dpimpex/save_video.h>
}
#endif


namespace {
static bool isSaveVideoFormatSupported(VideoFormat format, bool ffmpeg)
{
#ifdef DP_LIBAV
	bool (*predicate)(int) = ffmpeg ? DP_save_video_format_supported_ffmpeg
									: DP_save_video_format_supported;
	switch(format) {
	case VideoFormat::Gif:
		return predicate(DP_SAVE_VIDEO_FORMAT_PALETTE) &&
			   predicate(DP_SAVE_VIDEO_FORMAT_GIF);
	case VideoFormat::Webp:
		return predicate(DP_SAVE_VIDEO_FORMAT_WEBP);
	case VideoFormat::Mp4Vp9:
		return predicate(DP_SAVE_VIDEO_FORMAT_MP4_VP9);
	case VideoFormat::WebmVp8:
		return predicate(DP_SAVE_VIDEO_FORMAT_WEBM_VP8);
	case VideoFormat::Mp4H264:
		return predicate(DP_SAVE_VIDEO_FORMAT_MP4_H264);
	case VideoFormat::Mp4Av1:
		return predicate(DP_SAVE_VIDEO_FORMAT_MP4_AV1);
	default:
		break;
	}
#else
	Q_UNUSED(format);
	Q_UNUSED(predicate);
#endif
	return false;
}
}

bool isVideoFormatSupported(VideoFormat format)
{
	switch(format) {
#if !defined(Q_OS_ANDROID) && !defined(__EMSCRIPTEN__)
	case VideoFormat::Frames:
#endif
	case VideoFormat::Zip:
		return true;
	default:
		return isSaveVideoFormatSupported(format, false);
	}
}

bool isVideoFormatSupportedFfmpeg(VideoFormat format)
{
	return isSaveVideoFormatSupported(format, true);
}
