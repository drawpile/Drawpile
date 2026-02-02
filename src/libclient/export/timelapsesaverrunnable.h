// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_EXPORT_TIMELAPSESAVERRUNNABLE_H
#define LIBCLIENT_EXPORT_TIMELAPSESAVERRUNNABLE_H
#include "libclient/drawdance/canvasstate.h"
#include "libclient/drawdance/tile.h"
#include <QAtomicInt>
#include <QBrush>
#include <QColor>
#include <QImage>
#include <QObject>
#include <QQueue>
#include <QRect>
#include <QRunnable>

struct DP_DrawContext;
struct DP_Image;
struct DP_Mutex;
struct DP_Project;
struct DP_ProjectPlayback;
struct DP_Rect;
struct DP_SaveVideoNextFrame;
struct DP_Semaphore;
class QTemporaryFile;

class TimelapseSaverRunnable final : public QObject, public QRunnable {
	Q_OBJECT
public:
	TimelapseSaverRunnable(
		const drawdance::CanvasState &canvasState, const QString &outputPath,
		const QString &inputPath, int format, int width, int height,
		int interpolation, const QRect &crop, const QColor &backdropColor,
		const QColor &checkerColor1, const QColor &checkerColor2,
		const QColor &flashColor, const QRect &logoRect, double logoOpacity,
		const QImage &logoImage, double framerate, double lingerBeforeSeconds,
		double playbackSeconds, double flashSeconds, double lingerAfterSeconds,
		double maxDeltaSeconds, int maxQueueEntries, bool timeOwnOnly,
		QObject *parent = nullptr);

	void run() override;

public Q_SLOTS:
	void cancelExport();

Q_SIGNALS:
	void stepChanged(const QString &message);
	void progress(int percent);
	void frameProgress(const QImage &img);
	void saveComplete(qint64 msecs);
	void saveCancelled();
	void saveFailed(const QString &errorMessage);

private:
	static constexpr int MAX_QUEUE_SIZE_LIMIT = 1024;

	struct PlaybackEntry {
		int result;
		int instances;
		QImage img;
	};

	class PlaybackRunnable : public QRunnable {
	public:
		explicit PlaybackRunnable(TimelapseSaverRunnable *parent);

		void run() override;

		void requestStop() { m_stopRequested.storeRelaxed(1); }

	private:
		bool shouldStop() const
		{
			return m_parent->m_cancelled.loadRelaxed() != 0 ||
				   m_stopRequested.loadRelaxed() != 0;
		}

		void scaleLogoImage(const QImage &img);

		QImage toOutputImage(
			const drawdance::CanvasState &canvasState,
			const DP_Rect *cropOrNull);

		static bool imageHasTransparency(DP_Image *img);

		const QBrush &
		getBackgroundBrush(const drawdance::CanvasState &canvasState);

		QColor mixBackgroundColors(
			const QColor &bottom, const QColor &top, int topAlpha);

		bool handle(
			int instances, const drawdance::CanvasState &canvasState,
			const DP_Rect *cropOrNull);

		static bool handleCallback(
			void *user, int instances, DP_CanvasState *cs,
			const DP_Rect *cropOrNull);

		void enqueueLastImage(int instances);
		void enqueueFlashImage(double brightness);
		void enqueue(PlaybackEntry &&entry);

		TimelapseSaverRunnable *m_parent;
		QImage m_lastImage;
		QImage m_scaledLogoImage;
		QBrush m_lastBackgroundBrush;
		drawdance::Tile m_lastBackgroundTile = drawdance::Tile::null();
		QAtomicInt m_stopRequested;
	};

	bool checkParameters(QString &outErrorMessage) const;

	bool
	copyToTempFile(QTemporaryFile &tempFile, QString &outErrorMessage) const;

	DP_Project *
	openProject(const QTemporaryFile &tempFile, QString &outErrorMessage) const;

	DP_ProjectPlayback *
	openProjectPlayback(DP_Project *prj, QString &outErrorMessage) const;

	bool saveVideo(QString &outErrorMessage);

	PlaybackRunnable *startPlaybackThread();

	void finishPlaybackThread(PlaybackRunnable *pr);

	void closeProjectPlayback();

	void tryCloseProject(DP_Project *prj) const;

	int formatToSaveVideoFormat() const;

	bool isCancelled() const { return m_cancelled.loadRelaxed() != 0; }

	bool handleNextFrame(DP_SaveVideoNextFrame &f);

	static bool handleNextFrameCallback(void *user, DP_SaveVideoNextFrame *f);

	bool handleProgress(double value);

	static bool handleProgressCallback(void *user, double value);

	const drawdance::CanvasState m_canvasState;
	const QString m_outputPath;
	const QString m_inputPath;
	const int m_format;
	const int m_width;
	const int m_height;
	const int m_interpolation;
	const QRect m_crop;
	const QColor m_backdropColor;
	const QColor m_checkerColor1;
	const QColor m_checkerColor2;
	const QColor m_flashColor;
	const QRect m_logoRect;
	const double m_logoOpacity;
	const QImage m_logoImage;
	const double m_framerate;
	const double m_lingerBeforeSeconds;
	const double m_playbackSeconds;
	const double m_flashSeconds;
	const double m_lingerAfterSeconds;
	const double m_totalSeconds;
	const double m_maxDeltaSeconds;
	const int m_maxQueueEntries;
	const unsigned int m_playbackFlags;
	QImage m_scaledLogoImage;
	int m_elapsedFrames = 0;
	DP_DrawContext *m_dc = nullptr;
	DP_ProjectPlayback *m_projectPlayback = nullptr;
	DP_Mutex *m_mutex = nullptr;
	DP_Semaphore *m_availableSem = nullptr;
	DP_Semaphore *m_filledSem = nullptr;
	QQueue<PlaybackEntry> m_queue;
	QImage m_lastImage;
	bool m_playbackFinished = false;
	QAtomicInt m_cancelled;
};

#endif
