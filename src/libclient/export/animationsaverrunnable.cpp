// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/geom.h>
#include <dpimpex/save.h>
#ifdef DP_LIBAV
#	include <dpimpex/save_video.h>
#endif
}
#include "libclient/drawdance/global.h"
#include "libclient/export/animationformat.h"
#include "libclient/export/animationsaverrunnable.h"
#include "libclient/export/canvassaverrunnable.h"
#ifdef __EMSCRIPTEN__
#	include <QDateTime>
#	include <QFile>
#endif

AnimationSaverRunnable::AnimationSaverRunnable(
#ifndef __EMSCRIPTEN__
	const QString &path,
#endif
	int format, int width, int height, int loops, int start, int end,
	int framerate, const QRect &crop, bool scaleSmooth,
	const drawdance::CanvasState &canvasState, QObject *parent)
	: QObject(parent)
#ifndef __EMSCRIPTEN__
	, m_path(path)
#endif
	, m_format(format)
	, m_width(width)
	, m_height(height)
	, m_loops(loops)
	, m_start(start)
	, m_end(end)
	, m_framerate(framerate)
	, m_crop(crop)
	, m_canvasState(canvasState)
	, m_scaleSmooth(scaleSmooth)
	, m_cancelled(false)
{
}

void AnimationSaverRunnable::run()
{
#ifdef __EMSCRIPTEN__
	QString extension = getFormatExtension();
	QString path = QStringLiteral("/anim%1.%2")
					   .arg(QDateTime::currentMSecsSinceEpoch())
					   .arg(extension);
	QByteArray pathBytes = path.toUtf8();
#else
	QByteArray pathBytes = m_path.toUtf8();
#endif

	DP_Rect r, *pr;
	QRect crop = m_crop & QRect(QPoint(0, 0), m_canvasState.size());
	if(crop.isEmpty()) {
		pr = nullptr;
	} else {
		r = DP_rect_make(crop.x(), crop.y(), crop.width(), crop.height());
		pr = &r;
	}

	DP_SaveResult result;
	switch(m_format) {
#if !defined(__EMSCRIPTEN__) && !defined(Q_OS_ANDROID)
	case int(AnimationFormat::Frames): {
		drawdance::DrawContext dc = drawdance::DrawContextPool::acquire();
		result = DP_save_animation_frames(
			m_canvasState.get(), dc.get(), pathBytes.constData(), pr, m_width,
			m_height,
			m_scaleSmooth ? DP_MSG_TRANSFORM_REGION_MODE_BILINEAR
						  : DP_MSG_TRANSFORM_REGION_MODE_NEAREST,
			m_start, m_end, &onProgress, this);
		break;
	}
#endif
	case int(AnimationFormat::Zip): {
		drawdance::DrawContext dc = drawdance::DrawContextPool::acquire();
		result = DP_save_animation_zip(
			m_canvasState.get(), dc.get(), pathBytes.constData(), pr, m_width,
			m_height,
			m_scaleSmooth ? DP_MSG_TRANSFORM_REGION_MODE_BILINEAR
						  : DP_MSG_TRANSFORM_REGION_MODE_NEAREST,
			m_start, m_end, &onProgress, this);
		break;
	}
#ifdef DP_LIBAV
	case int(AnimationFormat::Gif): {
		DP_SaveGifParams params = {
			m_canvasState.get(),
			pr,
			DP_SAVE_VIDEO_DESTINATION_PATH,
			const_cast<char *>(pathBytes.constData()),
			m_scaleSmooth ? DP_SAVE_VIDEO_FLAGS_SCALE_SMOOTH
						  : DP_SAVE_VIDEO_FLAGS_NONE,
			m_width,
			m_height,
			m_start,
			m_end,
			m_framerate,
			&onProgress,
			this,
		};
		result = DP_save_animation_video_gif(params);
		break;
	}
	case int(AnimationFormat::Webp):
	case int(AnimationFormat::Mp4):
	case int(AnimationFormat::Webm): {
		DP_SaveVideoParams params = {
			m_canvasState.get(),
			pr,
			DP_SAVE_VIDEO_DESTINATION_PATH,
			const_cast<char *>(pathBytes.constData()),
			nullptr,
			0,
			m_scaleSmooth ? DP_SAVE_VIDEO_FLAGS_SCALE_SMOOTH
						  : DP_SAVE_VIDEO_FLAGS_NONE,
			formatToSaveVideoFormat(),
			m_width,
			m_height,
			m_start,
			m_end,
			m_framerate,
			m_loops,
			&onProgress,
			this,
		};
		result = DP_save_animation_video(params);
		break;
	}
#endif
	default:
		DP_error_set("Unhandled animation format %d", m_format);
		result = DP_SAVE_RESULT_UNKNOWN_FORMAT;
		break;
	}

	if(result != DP_SAVE_RESULT_SUCCESS) {
		qWarning("Error %d saving animation: %s", int(result), DP_error());
	}

#ifdef __EMSCRIPTEN__
	if(result == DP_SAVE_RESULT_SUCCESS) {
		QFile f(path);
		if(f.open(QIODevice::ReadOnly)) {
			emit downloadReady(
				QStringLiteral("animation%1").arg(extension), f.readAll());
		} else {
			result = DP_SAVE_RESULT_OPEN_ERROR;
		}
	}
	QFile::remove(path);
#else
	emit saveComplete(saveResultToErrorString(result));
#endif
}

void AnimationSaverRunnable::cancelExport()
{
	m_cancelled = true;
}

#ifdef __EMSCRIPTEN__
QString AnimationSaverRunnable::getFormatExtension() const
{
	switch(m_format) {
	case int(AnimationFormat::Gif):
		return QStringLiteral(".gif");
	case int(AnimationFormat::Zip):
		return QStringLiteral(".zip");
	case int(AnimationFormat::Webp):
		return QStringLiteral(".webp");
	case int(AnimationFormat::Mp4):
		return QStringLiteral(".mp4");
	case int(AnimationFormat::Webm):
		return QStringLiteral(".webm");
	default:
		qWarning("Don't know extension for animation format %d", m_format);
		return QString();
	}
}
#endif

#ifdef DP_LIBAV
int AnimationSaverRunnable::formatToSaveVideoFormat() const
{
	switch(m_format) {
	case int(AnimationFormat::Webp):
		return DP_SAVE_VIDEO_FORMAT_WEBP;
	case int(AnimationFormat::Mp4):
		return DP_SAVE_VIDEO_FORMAT_MP4;
	case int(AnimationFormat::Webm):
		return DP_SAVE_VIDEO_FORMAT_WEBM;
	default:
		qWarning("formatToSaveVideoFormat: unhandled format %d", m_format);
		return -1;
	}
}
#endif

QString
AnimationSaverRunnable::saveResultToErrorString(DP_SaveResult result) const
{
	if(result == DP_SAVE_RESULT_BAD_DIMENSIONS) {
		switch(m_format) {
		case int(AnimationFormat::Frames):
		case int(AnimationFormat::Zip):
			return CanvasSaverRunnable::badDimensionsErrorString(
				2147483647, QStringLiteral("PNG"));
		case int(AnimationFormat::Gif):
			return CanvasSaverRunnable::badDimensionsErrorString(
				65536, QStringLiteral("GIF"));
		case int(AnimationFormat::Webp):
			return CanvasSaverRunnable::badDimensionsErrorString(
				16384, QStringLiteral("WEBP"));
		case int(AnimationFormat::Mp4):
			return CanvasSaverRunnable::badDimensionsErrorString(
				2147483646, QStringLiteral("MP4"));
		case int(AnimationFormat::Webm):
			return CanvasSaverRunnable::badDimensionsErrorString(
				65536, QStringLiteral("WEBM"));
		}
	}
	return CanvasSaverRunnable::saveResultToErrorString(result);
}

bool AnimationSaverRunnable::onProgress(void *user, double progress)
{
	AnimationSaverRunnable *saver = static_cast<AnimationSaverRunnable *>(user);
	if(saver->m_cancelled) {
		return false;
	} else {
		emit saver->progress(qRound(progress * 100));
		return true;
	}
}
