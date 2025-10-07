// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_FLIPBOOK_H
#define DESKTOP_DIALOGS_FLIPBOOK_H
#include "libclient/drawdance/canvasstate.h"
#include <QDialog>
#include <QVector>

class QAction;
class QEvent;
class QPixmap;

namespace canvas {
class PaintEngine;
}

namespace utils {
class AnimationRenderer;
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
		int lastCanvasFrameRangeFirst = -1;
		int lastCanvasFrameRangeLast = -1;
		QSize lastCanvasSize;
		QPoint lastCanvasOffset;
	};

	explicit Flipbook(State &state, QWidget *parent = nullptr);
	~Flipbook() override;

	void setPaintEngine(canvas::PaintEngine *pe, const QRect &crop = QRect());
	void setRefreshShortcuts(const QList<QKeySequence> &shortcuts);

	bool event(QEvent *event) override;

signals:
	void stateChanged();
	void exportRequested();

private slots:
	void insertRenderedFrames(
		unsigned int batchId, const QVector<int> &frameIndexes,
		const QPixmap &frame);
	void nextFrame();
	void loadFrame();
	void playPause();
	void rewind();
	void updateSpeed();
	void updateRange();
	void setCrop(const QRectF &rect);
	void resetCrop();
	void refreshCanvas();

private:
	void resetCanvas(bool refresh, const QRect &crop);
	void updateSpeedSuffix();
	int getTimerInterval() const;
	void renderFrames();

	struct Private;
	Private *d;
};

}

#endif // FLIPBOOK_H
