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
static bool
isSaveVideoFormatSupported(VideoFormat format, bool (*predicate)(int))
{
#ifdef DP_LIBAV
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
			isVideoFormatSupportedFfmpeg(format),
			isVideoFormatSupportedNonFfmpeg(format),
		});
	}
}
}

bool isVideoFormatSupportedFfmpeg(VideoFormat format)
{
	return isSaveVideoFormatSupported(
		format, DP_save_video_format_supported_ffmpeg);
}

bool isVideoFormatSupportedNonFfmpeg(VideoFormat format)
{
	switch(format) {
	case VideoFormat::Frames:
#if defined(Q_OS_ANDROID) || defined(__EMSCRIPTEN__)
		return false;
#endif
	case VideoFormat::Zip:
		return true;
	default:
		return isSaveVideoFormatSupported(
			format, DP_save_video_format_supported_non_ffmpeg);
	}
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

namespace {
static void addVideoEncoderOption(
	QVector<VideoEncoderOption> &options, const DP_SaveVideoSupportEntry *entry)
{
	if(entry) {
		QString name = QString::fromUtf8(entry->name);
		QString suffix;
		if(!name.isEmpty()) {
			suffix = QStringLiteral(" - %1").arg(name);
		}

		int type = entry->type;
		switch(type) {
		case DP_SAVE_VIDEO_ENCODER_TYPE_LIBAV:
			options.append(
				{QStringLiteral("libav:") + name,
				 QStringLiteral("libav") + suffix, type});
			return;
		case DP_SAVE_VIDEO_ENCODER_TYPE_FFMPEG:
			options.append(
				{QStringLiteral("ffmpeg:") + name,
				 QStringLiteral("Ffmpeg") + suffix, type});
			return;
		case DP_SAVE_VIDEO_ENCODER_TYPE_ANDROID_SOFTWARE:
			options.append(
				{QStringLiteral("androidsoftware:") + name,
				 QStringLiteral("Android") + suffix, type});
			return;
		case DP_SAVE_VIDEO_ENCODER_TYPE_ANDROID_HARDWARE:
			options.append(
				{QStringLiteral("androidhardware:") + name,
				 QStringLiteral("Hardware") + suffix, type});
			return;
		case DP_SAVE_VIDEO_ENCODER_TYPE_WINDOWS:
			options.append(
				{QStringLiteral("windows:") + name,
				 QStringLiteral("Windows") + suffix, type});
			return;
		}
		DP_warn("Unhandled encoder type %d", type);
	}
}
}

QVector<VideoEncoderOption> getVideoEncoderOptions(VideoFormat format)
{
	DP_SaveVideoSupport *support;
	switch(format) {
	case VideoFormat::Webp:
		support = DP_save_video_format_support(DP_SAVE_VIDEO_FORMAT_WEBP);
		break;
	case VideoFormat::Mp4Vp9:
		support = DP_save_video_format_support(DP_SAVE_VIDEO_FORMAT_MP4_VP9);
		break;
	case VideoFormat::WebmVp8:
		support = DP_save_video_format_support(DP_SAVE_VIDEO_FORMAT_WEBM_VP8);
		break;
	case VideoFormat::Mp4H264:
		support = DP_save_video_format_support(DP_SAVE_VIDEO_FORMAT_MP4_H264);
		break;
	case VideoFormat::Mp4Av1:
		support = DP_save_video_format_support(DP_SAVE_VIDEO_FORMAT_MP4_AV1);
		break;
	case VideoFormat::Apng:
		support = DP_save_video_format_support(DP_SAVE_VIDEO_FORMAT_APNG);
		break;
	default:
		support = nullptr;
		break;
	}

	QVector<VideoEncoderOption> options;
	if(support) {
		int count = DP_save_video_support_count(support);
		options.reserve(count);
		for(int i = 0; i < count; ++i) {
			addVideoEncoderOption(
				options, DP_save_video_support_entry(support, i));
		}
	}
	return options;
}

int getAutomaticVideoEncoderOptionIndex(
	const QVector<VideoEncoderOption> &options, bool haveFfmpeg)
{
	int types[] = {
		DP_SAVE_VIDEO_ENCODER_TYPE_FFMPEG,
		DP_SAVE_VIDEO_ENCODER_TYPE_ANDROID_SOFTWARE,
		DP_SAVE_VIDEO_ENCODER_TYPE_ANDROID_HARDWARE,
		DP_SAVE_VIDEO_ENCODER_TYPE_WINDOWS,
		DP_SAVE_VIDEO_ENCODER_TYPE_LIBAV,
	};
	int count = options.size();
	for(int type : types) {
		if(haveFfmpeg || type != DP_SAVE_VIDEO_ENCODER_TYPE_FFMPEG) {
			for(int i = 0; i < count; ++i) {
				if(options[i].internalType == type) {
					return i;
				}
			}
		}
	}
	return 0;
}
