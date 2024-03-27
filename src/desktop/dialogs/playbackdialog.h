// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_PLAYBACKDIALOG_H
#define DESKTOP_DIALOGS_PLAYBACKDIALOG_H
extern "C" {
#include <dpengine/player.h>
}
#include <QDialog>
#include <QElapsedTimer>
#include <QPointer>
#include <functional>

namespace canvas {
class CanvasModel;
class PaintEngine;
}

class Ui_PlaybackDialog;
#ifdef HAVE_VIDEO_EXPORT
class VideoExporter;
#endif

namespace dialogs {

class PlaybackDialog final : public QDialog {
	Q_OBJECT
public:
	explicit PlaybackDialog(
		canvas::CanvasModel *canvas, QWidget *parent = nullptr);
	~PlaybackDialog() override;

	void centerOnParent();

	bool isPlaying() const;
	void setPlaying(bool playing);

signals:
	void playbackToggled(bool play);

protected:
	void closeEvent(QCloseEvent *) override;
	void keyPressEvent(QKeyEvent *) override;

private slots:
	void skipBeginning();
	void skipPreviousSnapshot();
	void skipNextStroke();
	void skipForward();

	void onPlaybackAt(long long pos);
#ifdef HAVE_VIDEO_EXPORT
	void onExporterReady();
#endif
	void playNext(double msecs);
	void jumpTo(int pos);

	void loadIndex();

	void onBuildIndexClicked();
#ifdef HAVE_VIDEO_EXPORT
	void onVideoExportClicked();
	void videoExporterError(const QString &msg);
	void videoExporterFinished(bool showExportDialogAgain);
	void exportFrame(int count = 1);
#endif

private:
	double getNextMsecs();
	void playbackCommand(std::function<DP_PlayerResult()> fn);
	void updateButtons();
	static bool isErrorResult(DP_PlayerResult result);

#ifdef HAVE_VIDEO_EXPORT
	void startVideoExport(VideoExporter *exporter);
#endif

	Ui_PlaybackDialog *m_ui;
	canvas::PaintEngine *m_paintengine;
#ifdef HAVE_VIDEO_EXPORT
	QPointer<VideoExporter> m_exporter;
#endif

	QTimer *m_playTimer;
	QElapsedTimer m_lastFrameTime;
	double m_speed;

	bool m_haveIndex;
	bool m_autoplay;
	bool m_awaiting;
};

}

#endif
