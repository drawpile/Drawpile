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
	explicit Flipbook(QWidget *parent = nullptr);
	~Flipbook() override;

	void setPaintEngine(canvas::PaintEngine *pe);

	bool event(QEvent *event) override;

private slots:
	void loadFrame();
	void playPause();
	void rewind();
	void updateSpeed();
	void updateRange();
	void setCrop(const QRectF &rect);
	void resetCrop();
	void refreshCanvas();

private:
	int getTimerInterval() const;
	void resetFrameCache();
	bool searchIdenticalFrame(int f, QPixmap &outFrame);

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
