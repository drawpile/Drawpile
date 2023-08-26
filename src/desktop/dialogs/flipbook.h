// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FLIPBOOK_H
#define FLIPBOOK_H

#include "libclient/drawdance/canvasstate.h"
#include "libclient/drawdance/viewmode.h"
#include <QDialog>
#include <QList>
#include <QPixmap>

class Ui_Flipbook;

class QEvent;
class QTimer;

namespace canvas {
class PaintEngine;
}

namespace dialogs {

class Flipbook final : public QDialog {
	Q_OBJECT
public:
	struct State {
		double speedPercent = -1.0;
		int loopStart = -1;
		int loopEnd = -1;
		QRectF crop;
		QSize lastCanvasSize;
		QPoint lastCanvasOffset;
	};

	explicit Flipbook(State &state, QWidget *parent = nullptr);
	~Flipbook() override;

	void setPaintEngine(canvas::PaintEngine *pe);

	bool event(QEvent *event) override;

signals:
	void exportGifRequested(
		const drawdance::CanvasState &canvasState, const QRect &crop, int start,
		int end, int framerate);

#ifndef Q_OS_ANDROID
	void exportFramesRequested(
		const drawdance::CanvasState &canvasState, const QRect &crop, int start,
		int end);
#endif

private slots:
	void loadFrame();
	void playPause();
	void rewind();
	void updateSpeed();
	void updateRange();
	void setCrop(const QRectF &rect);
	void resetCrop();
	void refreshCanvas();

private slots:
	void exportGif();
#ifndef Q_OS_ANDROID
	void exportFrames();
#endif

private:
	void resetCanvas(bool refresh);
	int getTimerInterval() const;
	void resetFrameCache();
	bool searchIdenticalFrame(int f, QPixmap &outFrame);
	QRect getExportRect() const;
	int getExportStart() const;
	int getExportEnd() const;
	int getExportFramerate() const;

	State &m_state;
	Ui_Flipbook *m_ui;
	canvas::PaintEngine *m_paintengine;
	drawdance::CanvasState m_canvasState;
	drawdance::ViewModeBuffer m_vmb;
	QList<QPixmap> m_frames;
	QTimer *m_timer;
	QRect m_crop;
};

}

#endif // FLIPBOOK_H
