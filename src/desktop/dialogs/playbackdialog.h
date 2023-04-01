// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PLAYBACKDIALOG_H
#define PLAYBACKDIALOG_H

#include <QDialog>
#include <QPointer>
#include <QElapsedTimer>

namespace canvas {
	class CanvasModel;
	class PaintEngine;
}

class Ui_PlaybackDialog;
class QMenu;
class VideoExporter;

namespace dialogs {

class PlaybackDialog final : public QDialog
{
	Q_OBJECT
public:
	explicit PlaybackDialog(canvas::CanvasModel *canvas, QWidget *parent=nullptr);
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
	void onPlaybackAt(long long pos);
	void playNext();
	void stepNext();
	void jumpTo(int pos);

	void loadIndex();

	void onBuildIndexClicked();
#ifndef Q_OS_ANDROID
	void onVideoExportClicked();
#endif

	void exportFrame(int count=1);

private:
	Ui_PlaybackDialog *m_ui;
	canvas::PaintEngine *m_paintengine;
	void *m_index;
	QPointer<VideoExporter> m_exporter;

	QTimer *m_playTimer;
	QElapsedTimer m_lastFrameTime;
	float m_speed;

	qint32 m_intervalAfterExport;
	bool m_autoplay;
	bool m_awaiting;
	bool m_exporting;
};

}

#endif // PLAYBACKDIALOG_H
