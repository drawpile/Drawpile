/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2021 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
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
	void closeEvent(QCloseEvent *);
	void keyPressEvent(QKeyEvent *);

private slots:
	void onPlaybackAt(long long pos, int interval);
	void stepNext();
	void autoStepNext(int interval);
	void jumpTo(int pos);

	void loadIndex();

	void onBuildIndexClicked();
	void onVideoExportClicked();

	void exportFrame(int count=1);

private:
	Ui_PlaybackDialog *m_ui;
	canvas::PaintEngine *m_paintengine;
	void *m_index;
	QPointer<VideoExporter> m_exporter;

	QTimer *m_autoStepTimer;
	QElapsedTimer m_lastInterval;
	float m_speedFactor;

	qint32 m_intervalAfterExport;
	bool m_autoplay;
	bool m_awaiting;
};

}

#endif // PLAYBACKDIALOG_H
