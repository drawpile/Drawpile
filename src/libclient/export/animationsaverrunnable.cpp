// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/geom.h>
#include <dpengine/save.h>
#ifdef DP_LIBAV
#	include <dpengine/save_video.h>
#endif
}
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
	int format, int loops, int start, int end, int framerate, const QRect &crop,
	const drawdance::CanvasState &canvasState, QObject *parent)
	: QObject(parent)
#ifndef __EMSCRIPTEN__
	, m_path(path)
#endif
	, m_format(format)
	, m_loops(loops)
	, m_start(start)
	, m_end(end)
	, m_framerate(framerate)
	, m_crop(crop)
	, m_canvasState(canvasState)
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
	case int(AnimationFormat::Frames):
		result = DP_save_animation_frames(
			m_canvasState.get(), pathBytes.constData(), pr, m_start, m_end,
			&onProgress, this);
		break;
#endif
	case int(AnimationFormat::Gif):
		result = DP_save_animation_gif(
			m_canvasState.get(), pathBytes.constData(), pr, m_start, m_end,
			m_framerate, &onProgress, this);
		break;
#ifdef DP_LIBAV
	case int(AnimationFormat::Webp):
	case int(AnimationFormat::Mp4):
	case int(AnimationFormat::Webm): {
		DP_SaveVideoParams params = {
			m_canvasState.get(),
			pr,
			pathBytes.constData(),
			formatToSaveVideoFormat(),
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
		qWarning("Unhandled animation format %d", m_format);
		result = DP_SAVE_RESULT_UNKNOWN_FORMAT;
		break;
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
	emit saveComplete(CanvasSaverRunnable::saveResultToErrorString(result));
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
