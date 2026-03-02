// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/export/videoformat.h"
#include <QCoreApplication>
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
	case VideoFormat::Apng:
		return predicate(DP_SAVE_VIDEO_FORMAT_APNG);
	default:
		break;
	}
#else
	Q_UNUSED(format);
	Q_UNUSED(ffmpeg);
#endif
	return false;
}

static void appendFormatOption(
	QVector<VideoFormatOption> &options, VideoFormatApplication application,
	VideoFormat format, const QString &title, bool forAnimation,
	bool forTimelapse)
{
	if((application == VideoFormatApplication::Animation && forAnimation) ||
	   (application == VideoFormatApplication::Timelapse && forTimelapse)) {
		options.append({
			format,
			title,
			isVideoFormatSupported(format),
			isVideoFormatSupportedFfmpeg(format),
		});
	}
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


QVector<VideoFormatOption> getVideoFormatOptions(
	VideoFormatApplication application, bool *outAnyFfmpegSupported)
{
	QVector<VideoFormatOption> options;
	appendFormatOption(
		options, application, VideoFormat::Frames,
		QCoreApplication::translate(
			"dialogs::AnimationExportDialog", "Frames as PNGs"),
		true, false);
	appendFormatOption(
		options, application, VideoFormat::Zip,
		QCoreApplication::translate(
			"dialogs::AnimationExportDialog", "Frames as PNGs in ZIP"),
		true, false);
	appendFormatOption(
		options, application, VideoFormat::Gif,
		QCoreApplication::translate(
			"dialogs::AnimationExportDialog", "Animated GIF"),
		true, false);
	appendFormatOption(
		options, application, VideoFormat::Webp,
		QCoreApplication::translate(
			"dialogs::AnimationExportDialog", "Animated WEBP"),
		true, false);
	appendFormatOption(
		options, application, VideoFormat::Apng,
		QCoreApplication::translate(
			"dialogs::AnimationExportDialog", "Animated PNG (APNG)"),
		true, false);
	appendFormatOption(
		options, application, VideoFormat::Mp4H264,
		QCoreApplication::translate(
			"dialogs::AnimationExportDialog", "MP4 Video (H.264)"),
		true, true);
	appendFormatOption(
		options, application, VideoFormat::Mp4Av1,
		QCoreApplication::translate(
			"dialogs::AnimationExportDialog", "MP4 Video (AV1)"),
		true, true);
	appendFormatOption(
		options, application, VideoFormat::Mp4Vp9,
		QCoreApplication::translate(
			"dialogs::AnimationExportDialog", "MP4 Video (VP9)"),
		true, true);
	appendFormatOption(
		options, application, VideoFormat::WebmVp8,
		QCoreApplication::translate(
			"dialogs::AnimationExportDialog", "WEBM Video (VP8)"),
		true, true);

	if(outAnyFfmpegSupported) {
		bool anyFfmpegSupported = false;
		for(const VideoFormatOption &vfo : options) {
			if(vfo.ffmpegSupported) {
				anyFfmpegSupported = true;
				break;
			}
		}
		*outAnyFfmpegSupported = anyFfmpegSupported;
	}

	return options;
}
