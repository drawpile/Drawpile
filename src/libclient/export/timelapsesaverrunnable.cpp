// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/geom.h>
#include <dpcommon/threading.h>
#include <dpengine/canvas_state.h>
#include <dpengine/image.h>
#include <dpengine/local_state.h>
#include <dpengine/pixels.h>
#include <dpengine/project.h>
#include <dpimpex/save_video.h>
}
#include "libclient/drawdance/geom.h"
#include "libclient/drawdance/global.h"
#include "libclient/drawdance/image.h"
#include "libclient/export/canvassaverrunnable.h"
#include "libclient/export/timelapsesaverrunnable.h"
#include "libclient/export/videoformat.h"
#include "libshared/util/paths.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QLoggingCategory>
#include <QPainter>
#include <QTemporaryFile>
#include <QThreadPool>
#include <cmath>

Q_LOGGING_CATEGORY(
	lcDpTimelapseSaverRunnable, "net.drawpile.export.timelapsesaverrunnable",
	QtWarningMsg)

TimelapseSaverRunnable::TimelapseSaverRunnable(
	const drawdance::CanvasState &canvasState,
	const DP_ViewModeFilter *vmfOrNull, const QString &outputPath,
	const QString &inputPath, int format, int width, int height,
	int interpolation, const QRect &crop, const QColor &backdropColor,
	const QColor &checkerColor1, const QColor &checkerColor2,
	const QColor &flashColor, const QRect &logoRect, double logoOpacity,
	const QImage &logoImage, double framerate, double lingerBeforeSeconds,
	double playbackSeconds, double flashSeconds, double lingerAfterSeconds,
	double maxDeltaSeconds, int maxQueueEntries, bool timeOwnOnly,
	QObject *parent)
	: QObject(parent)
	, m_canvasState(canvasState)
	, m_outputPath(outputPath)
	, m_inputPath(inputPath)
	, m_format(format)
	, m_width(width)
	, m_height(height)
	, m_interpolation(interpolation)
	, m_crop(crop)
	, m_backdropColor(backdropColor)
	, m_checkerColor1(checkerColor1)
	, m_checkerColor2(checkerColor2)
	, m_flashColor(flashColor)
	, m_logoRect(logoRect)
	, m_logoOpacity(qBound(0.0, logoOpacity, 1.0))
	, m_logoImage(
		  QRect(0, 0, width, height).intersected(logoRect).isEmpty() ||
				  m_logoOpacity <= 0.0
			  ? QImage()
			  : logoImage)
	, m_framerate(framerate)
	, m_lingerBeforeSeconds(lingerBeforeSeconds)
	, m_playbackSeconds(playbackSeconds)
	, m_flashSeconds(flashSeconds)
	, m_lingerAfterSeconds(lingerAfterSeconds)
	, m_totalSeconds(
		  lingerBeforeSeconds + playbackSeconds + flashSeconds +
		  lingerAfterSeconds)
	, m_maxDeltaSeconds(maxDeltaSeconds)
	, m_maxQueueEntries(maxQueueEntries)
	, m_playbackFlags(
		  DP_flag_uint(timeOwnOnly, DP_PROJECT_PLAYBACK_FLAG_MEASURE_OWN_ONLY))
	, m_vmf(DP_view_mode_filter_clone(m_vmb.get(), vmfOrNull))
{
}

void TimelapseSaverRunnable::run()
{
	QElapsedTimer timer;
	timer.start();

	if(isCancelled()) {
		Q_EMIT saveCancelled();
		return;
	}

	QString errorMessage;
	if(!checkParameters(errorMessage)) {
		Q_EMIT saveFailed(errorMessage);
		return;
	}

	Q_EMIT stepChanged(tr("Loading project…"));

	QTemporaryFile tempFile;
	if(!copyToTempFile(tempFile, errorMessage)) {
		Q_EMIT saveFailed(errorMessage);
		return;
	}

	if(isCancelled()) {
		Q_EMIT saveCancelled();
		return;
	}

	DP_Project *prj = openProject(tempFile, errorMessage);
	if(!prj) {
		Q_EMIT saveFailed(errorMessage);
		return;
	}

	if(isCancelled()) {
		tryCloseProject(prj);
		Q_EMIT saveCancelled();
		return;
	}

	Q_EMIT stepChanged(tr("Calculating…"));

	bool saveOk;
	{
		drawdance::DrawContext drawContext =
			drawdance::DrawContextPool::acquire();
		m_dc = drawContext.get();
		m_projectPlayback = openProjectPlayback(prj, errorMessage);
		if(!m_projectPlayback) {
			tryCloseProject(prj);
			Q_EMIT saveFailed(errorMessage);
			return;
		}

		if(isCancelled()) {
			closeProjectPlayback();
			tryCloseProject(prj);
			Q_EMIT saveCancelled();
			return;
		}

		saveOk = saveVideo(errorMessage);
		m_dc = nullptr;
	}
	Q_EMIT stepChanged(tr("Finishing up…"));
	closeProjectPlayback();
	tryCloseProject(prj);

	if(isCancelled()) {
		Q_EMIT saveCancelled();
		return;
	}

	if(!saveOk) {
		Q_EMIT saveFailed(errorMessage);
		return;
	}

	Q_EMIT saveComplete(timer.elapsed());
}

void TimelapseSaverRunnable::cancelExport()
{
	m_cancelled.storeRelaxed(1);
}


TimelapseSaverRunnable::PlaybackRunnable::PlaybackRunnable(
	TimelapseSaverRunnable *parent)
	: m_parent(parent)
{
}

void TimelapseSaverRunnable::PlaybackRunnable::run()
{
	int result;
	if(shouldStop()) {
		result = 0;
	} else {
		Q_EMIT m_parent->stepChanged(
			QCoreApplication::translate(
				"TimelapseSaverRunnable", "Rendering preview…"));

		const QImage &logoImage = m_parent->m_logoImage;
		if(!logoImage.isNull()) {
			if(logoImage.format() == QImage::Format_ARGB32_Premultiplied) {
				scaleLogoImage(logoImage);
			} else {
				scaleLogoImage(logoImage.convertToFormat(
					QImage::Format_ARGB32_Premultiplied));
			}
		}

		QImage finalImage = toOutputImage(
			m_parent->m_canvasState,
			DP_project_playback_crop_or_null(m_parent->m_projectPlayback),
			&m_parent->m_vmf);
		if(finalImage.isNull()) {
			finalImage = drawdance::wrapImage(
				DP_image_new(m_parent->m_width, m_parent->m_height));
		}
		m_lastImage = finalImage;

		double framerate = m_parent->m_framerate;
		int lingerBeforeFrames =
			int(m_parent->m_lingerBeforeSeconds * framerate);
		if(lingerBeforeFrames > 0) {
			enqueueLastImage(lingerBeforeFrames);
		}

		Q_EMIT m_parent->stepChanged(
			QCoreApplication::translate(
				"TimelapseSaverRunnable", "Rendering timelapse…"));

		result = DP_project_playback_play(
			m_parent->m_projectPlayback, m_parent->m_dc, framerate,
			m_parent->m_playbackSeconds, &PlaybackRunnable::handleCallback,
			this);

		Q_EMIT m_parent->stepChanged(
			QCoreApplication::translate(
				"TimelapseSaverRunnable", "Rendering result…"));

		int flashFrames = int(m_parent->m_flashSeconds * framerate);
		if(flashFrames > 0) {
			int brightest = flashFrames / 8;
			for(int i = 0; i < brightest; ++i) {
				enqueueFlashImage(double(i) / double(brightest));
			}

			m_lastImage = finalImage;
			enqueueFlashImage(1.0);

			for(int i = brightest + 1; i < flashFrames; ++i) {
				enqueueFlashImage(
					double(flashFrames - i) / double(flashFrames - brightest));
			}
		} else {
			m_lastImage = finalImage;
		}

		int lingerAfterFrames = int(m_parent->m_lingerAfterSeconds * framerate);
		enqueueLastImage(qMax(1, lingerAfterFrames));
	}

	enqueue({result, -1, QImage()});
}

void TimelapseSaverRunnable::PlaybackRunnable::scaleLogoImage(const QImage &img)
{
	if(img.isNull() || img.size().isEmpty()) {
		qCWarning(lcDpTimelapseSaverRunnable, "Invalid logo image");
	} else {
		QRect logoRect = m_parent->m_logoRect;
		if(logoRect.isEmpty()) {
			qCWarning(lcDpTimelapseSaverRunnable, "Invalid logo rectangle");
		} else {
			DP_Image *scaledImg = DP_image_scale_pixels(
				img.width(), img.height(),
				reinterpret_cast<const DP_Pixel8 *>(img.constBits()),
				m_parent->m_dc, logoRect.width(), logoRect.height(),
				DP_image_scale_interpolation_supported(
					DP_IMAGE_SCALE_INTERPOLATION_LANCZOS)
					? DP_IMAGE_SCALE_INTERPOLATION_LANCZOS
					: DP_IMAGE_SCALE_INTERPOLATION_FAST_BILINEAR);
			if(scaledImg) {
				m_scaledLogoImage = drawdance::wrapImage(scaledImg);
			} else {
				qCWarning(
					lcDpTimelapseSaverRunnable, "Scaling logo failed: %s",
					DP_error());
			}
		}
	}
}

QImage TimelapseSaverRunnable::PlaybackRunnable::toOutputImage(
	const drawdance::CanvasState &canvasState, const DP_Rect *cropOrNull,
	DP_ViewModeFilter *vmfOrNull)
{
	if(canvasState.isNull()) {
		return QImage();
	}

	DP_Image *img = DP_canvas_state_to_flat_image(
		canvasState.get(), DP_FLAT_IMAGE_RENDER_FLAGS, cropOrNull, vmfOrNull);
	if(!img) {
		return QImage();
	}

	// Scale the image proportionally and pad with transparency as needed.
	int width = DP_image_width(img);
	int height = DP_image_height(img);
	int scaledWidth = m_parent->m_width;
	int scaledHeight = m_parent->m_height;
	if(width != scaledWidth || height != scaledHeight) {
		double xratio = double(scaledWidth) / double(width);
		double yratio = double(scaledHeight) / double(height);

		int targetWidth, targetHeight;
		if(std::fabs(xratio - yratio) < 0.01) {
			targetWidth = scaledWidth;
			targetHeight = scaledHeight;
		} else if(xratio <= yratio) {
			targetWidth = scaledWidth;
			targetHeight = int(double(scaledHeight) * xratio);
		} else {
			targetWidth = int(double(scaledWidth) * yratio);
			targetHeight = scaledHeight;
		}

		DP_Image *scaledImg = DP_image_scale(
			img, m_parent->m_dc, targetWidth, targetHeight,
			m_parent->m_interpolation);
		DP_image_free(img);
		if(!scaledImg) {
			return QImage();
		}

		if(targetWidth == scaledWidth && targetHeight == scaledHeight) {
			img = scaledImg;
		} else {
			img = DP_image_new_subimage(
				scaledImg, -((scaledWidth - targetWidth) / 2),
				-((scaledHeight - targetHeight) / 2), scaledWidth,
				scaledHeight);
			DP_free(scaledImg);
			if(!img) {
				return QImage();
			}
		}
	}

	bool hasTransparency = imageHasTransparency(img);
	bool hasLogo = !m_scaledLogoImage.isNull();
	QImage qimage = drawdance::wrapImage(img);

	if(hasTransparency || hasLogo) {
		QPainter painter(&qimage);
		painter.setPen(Qt::NoPen);

		if(hasTransparency) {
			painter.setBrush(getBackgroundBrush(canvasState));
			painter.setCompositionMode(
				QPainter::CompositionMode_DestinationOver);
			painter.drawRect(qimage.rect());
		}

		if(hasLogo) {
			painter.setBrush(Qt::NoBrush);
			painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
			painter.setOpacity(m_parent->m_logoOpacity);
			painter.drawImage(m_parent->m_logoRect, m_scaledLogoImage);
		}
	}

	return qimage;
}

bool TimelapseSaverRunnable::PlaybackRunnable::imageHasTransparency(
	DP_Image *img)
{
	int pixelCount = DP_image_width(img) * DP_image_height(img);
	const DP_Pixel8 *pixels = DP_image_pixels(img);
	for(int i = 0; i < pixelCount; ++i) {
		if(pixels[i].bytes.a != 255) {
			return true;
		}
	}
	return false;
}

const QBrush &TimelapseSaverRunnable::PlaybackRunnable::getBackgroundBrush(
	const drawdance::CanvasState &canvasState)
{
	// The backdrop color goes on top of any other color to let the user
	// override the automatic background color with something of their choosing.
	const QColor &backdropColor = m_parent->m_backdropColor;
	int backdropAlpha = backdropColor.isValid() ? backdropColor.alpha() : 0;
	if(backdropAlpha < 255) {
		DP_Tile *t = DP_canvas_state_background_tile_noinc(canvasState.get());
		bool needsNewBrush = m_lastBackgroundBrush.style() == Qt::NoBrush ||
							 m_lastBackgroundTile.get() != t;
		if(needsNewBrush) {
			// Only bother generating a checkerboard image if the background
			// color has any transparency that actually makes it visible.
			m_lastBackgroundTile = drawdance::Tile::inc(t);
			QColor color = m_lastBackgroundTile.singleColor(Qt::transparent);
			int alpha = color.isValid() ? color.alpha() : 0;
			if(alpha < 255) {
				QImage img(64, 64, QImage::Format_ARGB32_Premultiplied);
				QPainter painter(&img);
				// Checkerboard.
				painter.setRenderHint(QPainter::Antialiasing, false);
				painter.setCompositionMode(QPainter::CompositionMode_Source);
				painter.fillRect(0, 0, 32, 32, m_parent->m_checkerColor1);
				painter.fillRect(32, 0, 32, 32, m_parent->m_checkerColor2);
				painter.fillRect(0, 32, 32, 32, m_parent->m_checkerColor2);
				painter.fillRect(32, 32, 32, 32, m_parent->m_checkerColor1);
				painter.setCompositionMode(
					QPainter::CompositionMode_SourceOver);
				// Background color over the checkerboard, if applicable.
				if(alpha > 0 || backdropAlpha > 0) {
					painter.fillRect(
						0, 0, 64, 64,
						mixBackgroundColors(
							color, backdropColor, backdropAlpha));
				}
				m_lastBackgroundBrush = QBrush(img);
			} else if(backdropAlpha > 0) {
				// Just a solid color.
				m_lastBackgroundBrush = QBrush(
					mixBackgroundColors(color, backdropColor, backdropAlpha));
			}
		}
	} else if(m_lastBackgroundBrush.style() == Qt::NoBrush) {
		// Solid backdrop color over everything.
		m_lastBackgroundBrush = QBrush(backdropColor);
	}
	return m_lastBackgroundBrush;
}

QColor TimelapseSaverRunnable::PlaybackRunnable::mixBackgroundColors(
	const QColor &bottom, const QColor &top, int topAlpha)
{
	if(topAlpha > 0) {
		QImage img(1, 1, QImage::Format_ARGB32_Premultiplied);
		img.fill(bottom);
		{
			QPainter painter(&img);
			painter.fillRect(img.rect(), top);
		}
		return img.pixelColor(0, 0);
	} else {
		return bottom;
	}
}

bool TimelapseSaverRunnable::PlaybackRunnable::handle(
	int instances, const drawdance::CanvasState &canvasState, DP_LocalState *ls,
	const DP_Rect *cropOrNull)
{
	if(shouldStop()) {
		qCDebug(lcDpTimelapseSaverRunnable, "Playback thread cancelled");
		return false;
	}

	if(instances > 0) {
		DP_ViewModeFilter vmf = DP_view_mode_filter_make(
			m_parent->m_vmb.get(), DP_local_state_view_mode(ls),
			canvasState.get(), DP_local_state_active_layer_id(ls),
			DP_local_state_active_frame_index(ls),
			DP_local_state_onion_skins(ls));
		QImage img = toOutputImage(canvasState, cropOrNull, &vmf);
		if(img.isNull()) {
			qCWarning(
				lcDpTimelapseSaverRunnable, "Failed to create output image: %s",
				DP_error());
		} else {
			m_lastImage = img;
		}
		enqueue({0, instances, m_lastImage});
	} else {
		qCWarning(
			lcDpTimelapseSaverRunnable,
			"Invalid frame instance count %d (should be > 0)", instances);
	}
	return true;
}

bool TimelapseSaverRunnable::PlaybackRunnable::handleCallback(
	void *user, int instances, DP_CanvasState *cs, DP_LocalState *ls,
	const DP_Rect *cropOrNull)
{
	return static_cast<TimelapseSaverRunnable::PlaybackRunnable *>(user)
		->handle(instances, drawdance::CanvasState::noinc(cs), ls, cropOrNull);
}

void TimelapseSaverRunnable::PlaybackRunnable::enqueueLastImage(int instances)
{
	enqueue({0, instances, m_lastImage});
}

void TimelapseSaverRunnable::PlaybackRunnable::enqueueFlashImage(
	double brightness)
{
	int width = m_parent->m_width;
	int height = m_parent->m_height;
	DP_Image *dstImage = DP_image_new(width, height);
	DP_Pixel8 *dstPixels = DP_image_pixels(dstImage);
	uint8_t opacity = qBound(1, qRound(brightness * 254.0) + 1, 255);
	const DP_Pixel8 *srcPixels =
		reinterpret_cast<const DP_Pixel8 *>(m_lastImage.constBits());
	DP_UPixel8 color = {m_parent->m_flashColor.rgba()};
	DP_blend_color8_to(dstPixels, srcPixels, color, width * height, opacity);
	enqueue({0, 1, drawdance::wrapImage(dstImage)});
}

void TimelapseSaverRunnable::PlaybackRunnable::enqueue(PlaybackEntry &&entry)
{
	DP_SEMAPHORE_MUST_WAIT(m_parent->m_availableSem);
	DP_Mutex *mutex = m_parent->m_mutex;
	DP_MUTEX_MUST_LOCK(mutex);
	m_parent->m_queue.enqueue(std::move(entry));
	DP_SEMAPHORE_MUST_POST(m_parent->m_filledSem);
	DP_MUTEX_MUST_UNLOCK(mutex);
}


bool TimelapseSaverRunnable::checkParameters(QString &outErrorMessage) const
{
	QStringList errors;
	auto appendError = [&errors](const QString &error) {
		errors.append(QStringLiteral("<li>%1</li>").arg(error.toHtmlEscaped()));
	};

	if(m_inputPath.isEmpty()) {
		appendError(tr("No input path given."));
	}

	if(m_outputPath.isEmpty()) {
		appendError(tr("No output path given."));
	}

	switch(m_format) {
	case int(VideoFormat::Mp4Vp9):
	case int(VideoFormat::WebmVp8):
	case int(VideoFormat::Mp4H264):
		break;
	default:
		appendError(tr("Invalid output format %1 given.").arg(m_format));
		break;
	}

	if(!std::isfinite(m_framerate) || m_framerate <= 0.0) {
		appendError(tr("Invalid framerate %1 given.").arg(m_framerate));
	}

	if(!std::isfinite(m_lingerBeforeSeconds) ||
	   !std::isfinite(m_playbackSeconds) || !std::isfinite(m_flashSeconds) ||
	   !std::isfinite(m_lingerAfterSeconds) || !std::isfinite(m_totalSeconds) ||
	   !std::isfinite(m_maxDeltaSeconds) || m_lingerBeforeSeconds < 0.0 ||
	   m_playbackSeconds <= 0.0 || m_flashSeconds < 0.0 ||
	   m_lingerAfterSeconds < 0.0 || m_maxDeltaSeconds <= 0.0) {
		appendError(tr("Invalid time given."));
	}

	if(m_maxQueueEntries < 1 || m_maxQueueEntries > MAX_QUEUE_SIZE_LIMIT) {
		appendError(
			tr("Invalid frame queue size %1 given.").arg(m_maxQueueEntries));
	}

	int errorCount = int(errors.size());
	if(errorCount == 0) {
		return true;
	} else {
		outErrorMessage =
			QStringLiteral("<p>%1</p><ul>%2</ul>")
				.arg(
					tr("Invalid parameter(s):", nullptr, errorCount)
						.toHtmlEscaped(),
					errors.join(QString()));
		return false;
	}
}

bool TimelapseSaverRunnable::copyToTempFile(
	QTemporaryFile &tempFile, QString &outErrorMessage) const
{
	QFile inputFile(m_inputPath);
	if(!inputFile.open(QIODevice::ReadOnly)) {
		outErrorMessage =
			tr("Failed to open %1: %2")
				.arg(inputFile.fileName(), inputFile.errorString());
		return false;
	}

	if(!tempFile.open()) {
		outErrorMessage =
			tr("Failed to open temporary file: %1").arg(tempFile.errorString());
		return false;
	}

	qCDebug(
		lcDpTimelapseSaverRunnable, "Copying input file '%s' to temporary '%s'",
		qUtf8Printable(inputFile.fileName()),
		qUtf8Printable(tempFile.fileName()));

	bool ok =
		utils::paths::copyFileContents(inputFile, tempFile, outErrorMessage);
	tempFile.close();
	inputFile.close();
	return ok;
}

DP_Project *TimelapseSaverRunnable::openProject(
	const QTemporaryFile &tempFile, QString &outErrorMessage) const
{
	qCDebug(
		lcDpTimelapseSaverRunnable, "Opening project '%s'",
		qUtf8Printable(tempFile.fileName()));
	DP_ProjectOpenResult result = DP_project_open(
		tempFile.fileName().toUtf8().constData(), DP_PROJECT_OPEN_READ_ONLY);
	if(result.project) {
		return result.project;
	} else {
		qCWarning(
			lcDpTimelapseSaverRunnable,
			"Error %d (%d) opening project '%s': %s", result.error,
			result.sql_result, qUtf8Printable(tempFile.fileName()), DP_error());
		outErrorMessage =
			tr("Error %d opening project.").arg(QString::number(result.error));
		return nullptr;
	}
}

DP_ProjectPlayback *TimelapseSaverRunnable::openProjectPlayback(
	DP_Project *prj, QString &outErrorMessage) const
{
	DP_Rect crop;
	DP_Rect *cropOrNull;
	if(m_crop.isEmpty() || !drawdance::rectQtToDp(m_crop, crop)) {
		cropOrNull = nullptr;
	} else {
		cropOrNull = &crop;
	}

	DP_ProjectPlayback *pb = DP_project_playback_new(prj);
	int result = DP_project_playback_measure(
		pb, m_dc, m_maxDeltaSeconds, cropOrNull, m_playbackFlags);
	if(result != 0) {
		qCWarning(
			lcDpTimelapseSaverRunnable,
			"Error %d measuring project playback: %s", result, DP_error());
		if(result == DP_PROJECT_PLAYBACK_ERROR_EMPTY) {
			outErrorMessage =
				tr("Nothing to play back. You may not have recorded anything "
				   "to this project file.");
		} else {
			outErrorMessage = tr("Error reading playback timing.");
		}
		DP_project_playback_free(pb);
		return nullptr;
	}

	return pb;
}

bool TimelapseSaverRunnable::saveVideo(QString &outErrorMessage)
{
	PlaybackRunnable *pr = startPlaybackThread();
	if(!pr) {
		outErrorMessage = tr("Failed to start playback.");
		return false;
	}

	DP_SaveResult result;
	{
		QByteArray outputPathBytes = m_outputPath.toUtf8();
		DP_SaveVideoParams params = {
			DP_SAVE_VIDEO_DESTINATION_PATH,
			outputPathBytes.data(),
			nullptr,
			0,
			DP_SAVE_VIDEO_FLAGS_NONE,
			formatToSaveVideoFormat(),
			m_width,
			m_height,
			m_framerate,
			&TimelapseSaverRunnable::handleNextFrameCallback,
			&TimelapseSaverRunnable::handleProgressCallback,
			this,
		};
		result = DP_save_video(params);
	}

	finishPlaybackThread(pr);

	switch(result) {
	case DP_SAVE_RESULT_SUCCESS:
		return true;
	case DP_SAVE_RESULT_CANCEL:
		m_cancelled.storeRelaxed(true);
		return false;
	default:
		outErrorMessage = CanvasSaverRunnable::saveResultToErrorString(result);
		return false;
	}
}

TimelapseSaverRunnable::PlaybackRunnable *
TimelapseSaverRunnable::startPlaybackThread()
{
	m_mutex = DP_mutex_new();
	if(!m_mutex) {
		qCWarning(
			lcDpTimelapseSaverRunnable, "Failed to initialize mutex: %s",
			DP_error());
		return nullptr;
	}

	m_availableSem = DP_semaphore_new(m_maxQueueEntries);
	if(!m_availableSem) {
		qCWarning(
			lcDpTimelapseSaverRunnable,
			"Failed to initialize available semaphore: %s", DP_error());
		DP_mutex_free(m_mutex);
		return nullptr;
	}

	m_filledSem = DP_semaphore_new(0);
	if(!m_filledSem) {
		qCWarning(
			lcDpTimelapseSaverRunnable,
			"Failed to initialize filled semaphore: %s", DP_error());
		DP_semaphore_free(m_availableSem);
		DP_mutex_free(m_mutex);
		return nullptr;
	}

	PlaybackRunnable *pr = new PlaybackRunnable(this);
	pr->setAutoDelete(false);

	QThreadPool *threadPool = QThreadPool::globalInstance();
	bool started = threadPool->tryStart(pr);
	if(!started) {
		qCWarning(
			lcDpTimelapseSaverRunnable,
			"Failed to start playback runnable on thread pool, falling back to "
			"a new thread");
		m_thread = DP_thread_new(&TimelapseSaverRunnable::runPlayback, pr);
		if(!m_thread) {
			qCWarning(
				lcDpTimelapseSaverRunnable,
				"Failed to start playback thread: %s", DP_error());
			delete pr;
			DP_semaphore_free(m_filledSem);
			DP_semaphore_free(m_availableSem);
			DP_mutex_free(m_mutex);
			return nullptr;
		}
	}

	return pr;
}

void TimelapseSaverRunnable::finishPlaybackThread(PlaybackRunnable *pr)
{
	if(!m_playbackFinished) {
		pr->requestStop();
		qCDebug(
			lcDpTimelapseSaverRunnable,
			"Waiting for playback thread to finish");
		do {
			DP_SEMAPHORE_MUST_WAIT(m_filledSem);
			DP_MUTEX_MUST_LOCK(m_mutex);
			Q_ASSERT(!m_queue.isEmpty());
			PlaybackEntry pe = m_queue.dequeue();
			DP_SEMAPHORE_MUST_POST(m_availableSem);
			DP_MUTEX_MUST_UNLOCK(m_mutex);
			m_playbackFinished = pe.instances < 1;
		} while(!m_playbackFinished);
	}

	qCDebug(lcDpTimelapseSaverRunnable, "Cleaning up playback thread");
	DP_thread_free_join(m_thread);
	DP_semaphore_free(m_filledSem);
	DP_semaphore_free(m_availableSem);
	DP_mutex_free(m_mutex);
	delete pr;
	m_lastImage = QImage();
}

void TimelapseSaverRunnable::closeProjectPlayback()
{
	DP_project_playback_free(m_projectPlayback);
	m_projectPlayback = nullptr;
}

void TimelapseSaverRunnable::tryCloseProject(DP_Project *prj) const
{
	if(!DP_project_close(prj)) {
		qCWarning(
			lcDpTimelapseSaverRunnable, "Error closing project: %s",
			DP_error());
	}
}

int TimelapseSaverRunnable::formatToSaveVideoFormat() const
{
	switch(m_format) {
	case int(VideoFormat::Mp4Vp9):
		return int(DP_SAVE_VIDEO_FORMAT_MP4_VP9);
	case int(VideoFormat::WebmVp8):
		return int(DP_SAVE_VIDEO_FORMAT_WEBM_VP8);
	case int(VideoFormat::Mp4H264):
		return int(DP_SAVE_VIDEO_FORMAT_MP4_H264);
	default:
		qCWarning(
			lcDpTimelapseSaverRunnable, "No save video format for format %d",
			m_format);
		return -1;
	}
}

bool TimelapseSaverRunnable::handleNextFrame(DP_SaveVideoNextFrame &f)
{
	DP_SEMAPHORE_MUST_WAIT(m_filledSem);
	DP_MUTEX_MUST_LOCK(m_mutex);
	Q_ASSERT(!m_queue.isEmpty());
	PlaybackEntry pe = m_queue.dequeue();
	DP_SEMAPHORE_MUST_POST(m_availableSem);
	DP_MUTEX_MUST_UNLOCK(m_mutex);

	if(pe.instances > 0) {
		m_elapsedFrames += pe.instances;
		m_lastImage = std::move(pe.img);
		f.result = DP_SAVE_RESULT_SUCCESS;
		f.instances = pe.instances;
		f.width = m_lastImage.width();
		f.height = m_lastImage.height();
		f.pixels = m_lastImage.constBits();
		f.progress = (double(m_elapsedFrames) / m_framerate) / m_totalSeconds;
		Q_EMIT frameProgress(m_lastImage);
		return true;
	} else {
		m_playbackFinished = true;
		if(pe.result == 0) {
			f.result = DP_SAVE_RESULT_SUCCESS;
		} else {
			f.result = DP_SAVE_RESULT_INTERNAL_ERROR;
		}
		return false;
	}
}

bool TimelapseSaverRunnable::handleNextFrameCallback(
	void *user, DP_SaveVideoNextFrame *f)
{
	return static_cast<TimelapseSaverRunnable *>(user)->handleNextFrame(*f);
}

bool TimelapseSaverRunnable::handleProgress(double value)
{
	if(isCancelled()) {
		return false;
	} else {
		Q_EMIT progress(qRound(value * 100.0));
		return true;
	}
}

bool TimelapseSaverRunnable::handleProgressCallback(void *user, double value)
{
	return static_cast<TimelapseSaverRunnable *>(user)->handleProgress(value);
}

void TimelapseSaverRunnable::runPlayback(void *user)
{
	PlaybackRunnable *pr = static_cast<PlaybackRunnable *>(user);
	pr->run();
}
