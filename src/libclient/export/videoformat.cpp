// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/export/videoformat.h"
#include <QtGlobal>
#ifdef DP_LIBAV
extern "C" {
#	include <dpimpex/save_video.h>
}
#endif


bool isVideoFormatSupported(VideoFormat format)
{
	switch(format) {
#if !defined(Q_OS_ANDROID) && !defined(__EMSCRIPTEN__)
	case VideoFormat::Frames:
#endif
	case VideoFormat::Zip:
		return true;
#ifdef DP_LIBAV
	case VideoFormat::Gif:
		return DP_save_video_format_supported(DP_SAVE_VIDEO_FORMAT_PALETTE) &&
			   DP_save_video_format_supported(DP_SAVE_VIDEO_FORMAT_GIF);
	case VideoFormat::Mp4Vp9:
		return DP_save_video_format_supported(DP_SAVE_VIDEO_FORMAT_MP4_VP9);
	case VideoFormat::WebmVp8:
		return DP_save_video_format_supported(DP_SAVE_VIDEO_FORMAT_WEBM_VP8);
	case VideoFormat::Webp:
		return DP_save_video_format_supported(DP_SAVE_VIDEO_FORMAT_WEBP);
#endif
	default:
		return false;
	}
}
