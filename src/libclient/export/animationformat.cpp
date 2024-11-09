// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/export/animationformat.h"
#include <QtGlobal>
#ifdef DP_LIBAV
extern "C" {
#	include <dpimpex/save_video.h>
}
#endif


bool isAnimationFormatSupported(AnimationFormat format)
{
	switch(format) {
#if !defined(Q_OS_ANDROID) && !defined(__EMSCRIPTEN__)
	case AnimationFormat::Frames:
#endif
	case AnimationFormat::Zip:
		return true;
#ifdef DP_LIBAV
	case AnimationFormat::Gif:
		return DP_save_video_format_supported(DP_SAVE_VIDEO_FORMAT_PALETTE) &&
			   DP_save_video_format_supported(DP_SAVE_VIDEO_FORMAT_GIF);
	case AnimationFormat::Mp4:
		return DP_save_video_format_supported(DP_SAVE_VIDEO_FORMAT_MP4);
	case AnimationFormat::Webm:
		return DP_save_video_format_supported(DP_SAVE_VIDEO_FORMAT_WEBM);
	case AnimationFormat::Webp:
		return DP_save_video_format_supported(DP_SAVE_VIDEO_FORMAT_WEBP);
#endif
	default:
		return false;
	}
}
