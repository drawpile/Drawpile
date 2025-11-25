// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/view_mode.h>
}
#include "desktop/dialogs/flipbook.h"
#include "desktop/main.h"
#include "desktop/utils/animationrenderer.h"
#include "desktop/utils/qtguicompat.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/config/config.h"
#include "libclient/drawdance/viewmode.h"
#include "ui_flipbook.h"
#include <QAction>
#include <QEvent>
#include <QHash>
#include <QMenu>
#include <QPixmap>
#include <QRect>
#include <QScreen>
#include <QSignalBlocker>
#include <QTimer>

namespace dialogs {

struct Flipbook::Private {
	State &state;
	Ui_Flipbook ui;
	QAction *refreshAction;
	QAction *resetCropAction;
	QAction *upscaleAction;
	canvas::PaintEngine *paintengine = nullptr;
	drawdance::CanvasState canvasState;
	drawdance::ViewModeBuffer vmb;
	utils::AnimationRenderer *animationRenderer;
	QHash<int, QPixmap> frames;
	QTimer timer;
	QRect crop;
	unsigned int batchId = 0;
	bool stalled = false;
	bool skipped = false;

	Private(State &s)
		: state(s)
	{
	}
};

Flipbook::Flipbook(State &state, QWidget *parent)
	: QDialog(parent)
	, d(new Private(state))
{
	d->ui.setupUi(this);
	d->animationRenderer = new utils::AnimationRenderer(this);
	connect(
		d->animationRenderer, &utils::AnimationRenderer::frameRendered, this,
		&Flipbook::insertRenderedFrames, Qt::QueuedConnection);

	d->refreshAction = new QAction{this};
	addAction(d->refreshAction);
	connect(d->refreshAction, &QAction::triggered, this, [this] {
		d->ui.refreshButton->animateClick();
	});

	QMenu *zoomMenu = new QMenu(this);
	d->ui.zoomButton->setMenu(zoomMenu);

	d->resetCropAction = zoomMenu->addAction(tr("Reset crop"));
	connect(
		d->resetCropAction, &QAction::triggered, this, &Flipbook::resetCrop);

	d->upscaleAction = zoomMenu->addAction(tr("Upscale to fit view"));
	d->upscaleAction->setCheckable(true);
	config::Config *cfg = dpAppConfig();
	CFG_BIND_ACTION(cfg, FlipbookUpscaling, d->upscaleAction);
	CFG_BIND_SET(
		cfg, FlipbookUpscaling, d->ui.view, FlipbookView::setUpscaling);

	connect(d->ui.rewindButton, &QToolButton::clicked, this, &Flipbook::rewind);
	connect(
		d->ui.playButton, &QToolButton::clicked, this, &Flipbook::playPause);
	connect(
		d->ui.layerIndex, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&Flipbook::loadFrame);
	connect(
		d->ui.loopStart, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&Flipbook::updateRange);
	connect(
		d->ui.loopEnd, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&Flipbook::updateRange);
	connect(
		d->ui.speedSpinner,
		QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
		&Flipbook::updateSpeed);
	connect(&d->timer, &QTimer::timeout, this, &Flipbook::nextFrame);
	connect(d->ui.view, &FlipbookView::cropped, this, &Flipbook::setCrop);
	connect(
		d->ui.refreshButton, &QToolButton::clicked, this,
		&Flipbook::refreshCanvas);
	connect(
		d->ui.exportButton, &QToolButton::clicked, this,
		&Flipbook::exportRequested);
	connect(
		d->ui.buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	d->ui.speedSpinner->setExponentRatio(3.0);
	d->ui.playButton->setFocus();

	utils::setGeometryIfOnScreen(this, cfg->getFlipbookWindow());
}

Flipbook::~Flipbook()
{
	d->animationRenderer->detachDelete();
	delete d;
}

bool Flipbook::event(QEvent *event)
{
	switch(event->type()) {
	case QEvent::Move:
	case QEvent::Resize:
		dpAppConfig()->setFlipbookWindow(geometry());
		break;
	default: {
	}
	}

	return QDialog::event(event);
}

void Flipbook::updateRange()
{
	int loopStart = d->ui.loopStart->value();
	int loopEnd = d->ui.loopEnd->value();
	d->ui.layerIndex->setRange(loopStart, loopEnd);
	d->ui.layerIndex->setValue(loopStart);
	d->state.loopStart = loopStart;
	d->state.loopEnd = loopEnd;
	emit stateChanged();
}

void Flipbook::rewind()
{
	d->ui.layerIndex->setValue(d->ui.layerIndex->minimum());
}

void Flipbook::playPause()
{
	if(d->timer.isActive()) {
		d->timer.stop();
		d->ui.playButton->setIcon(QIcon::fromTheme("media-playback-start"));
	} else if(!d->canvasState.isNull()) {
		d->timer.start(getTimerInterval());
		d->ui.playButton->setIcon(QIcon::fromTheme("media-playback-pause"));
	}
}

void Flipbook::updateSpeed()
{
	double speedPercent = d->ui.speedSpinner->value();
	d->state.speedPercent = speedPercent;
	emit stateChanged();
	updateSpeedSuffix();
	if(d->timer.isActive()) {
		d->timer.setInterval(getTimerInterval());
	}
}

void Flipbook::updateSpeedSuffix()
{
	QString fpsSuffix;
	if(d->canvasState.isNull()) {
		fpsSuffix = tr("%");
	} else {
		double fps = d->canvasState.effectiveFramerate() *
					 d->ui.speedSpinner->value() / 100.0;
		fpsSuffix = tr("% (%1 FPS)").arg(fps, 0, 'f', 2);
	}
	d->ui.speedSpinner->setSuffix(fpsSuffix);
}

void Flipbook::setPaintEngine(canvas::PaintEngine *pe, const QRect &crop)
{
	d->paintengine = pe;
	resetCanvas(false, crop);
	if(!d->timer.isActive()) {
		playPause();
	}
}

void Flipbook::setRefreshShortcuts(const QList<QKeySequence> &shortcuts)
{
	d->refreshAction->setShortcuts(shortcuts);
}

void Flipbook::setCrop(const QRectF &rect)
{
	const int w = d->crop.width();
	const int h = d->crop.height();
	QSize canvasSize =
		d->canvasState.isNull() ? QSize{} : d->canvasState.size();
	QRect canvasRect(QPoint(), canvasSize);

	if(rect.width() * w <= 5 || rect.height() * h <= 5) {
		d->crop = canvasRect;
		d->resetCropAction->setEnabled(false);
	} else {
		d->crop = QRect(
					  d->crop.x() + rect.x() * w, d->crop.y() + rect.y() * h,
					  rect.width() * w, rect.height() * h)
					  .intersected(canvasRect);
		d->resetCropAction->setEnabled(true);
	}

	d->state.crop =
		d->crop == canvasRect
			? QRectF()
			: QRectF(
				  qreal(d->crop.x()) / qreal(canvasRect.width()),
				  qreal(d->crop.y()) / qreal(canvasRect.height()),
				  qreal(d->crop.width()) / qreal(canvasRect.width()),
				  qreal(d->crop.height()) / qreal(canvasRect.height()));
	d->state.lastCanvasOffset =
		d->canvasState.isNull() ? QPoint{} : d->canvasState.offset();
	d->state.lastCanvasSize = canvasSize;
	emit stateChanged();

	renderFrames();
}

void Flipbook::resetCrop()
{
	setCrop(QRectF());
}

void Flipbook::refreshCanvas()
{
	resetCanvas(true, QRect());
}

void Flipbook::resetCanvas(bool refresh, const QRect &crop)
{
	if(!d->paintengine) {
		return;
	}

	d->canvasState = d->paintengine->viewCanvasState();
	updateSpeedSuffix();
	if(d->canvasState.isNull()) {
		return;
	}

	if(d->state.speedPercent > 0.0) {
		d->ui.speedSpinner->setValue(d->state.speedPercent);
	} else {
		d->ui.speedSpinner->setValue(100.0);
	}

	QSignalBlocker loopStartBlocker{d->ui.loopStart};
	QSignalBlocker loopEndBlocker{d->ui.loopEnd};
	int frameCount = d->canvasState.frameCount();
	d->ui.loopStart->setMaximum(frameCount);
	d->ui.loopEnd->setMaximum(frameCount);

	int frameRangeFirst, frameRangeLast;
	d->canvasState.documentMetadata().effectiveFrameRange(
		frameRangeFirst, frameRangeLast);
	if(d->state.loopStart > 0 && d->state.loopEnd > 0) {
		d->ui.loopStart->setValue(
			d->ui.loopStart->value() == d->state.lastCanvasFrameRangeFirst + 1
				? frameRangeFirst + 1
				: d->state.loopStart);
		d->ui.loopEnd->setValue(
			d->ui.loopEnd->value() == d->state.lastCanvasFrameRangeLast + 1
				? frameRangeLast + 1
				: d->state.loopEnd);
	} else {
		d->ui.loopStart->setValue(frameRangeFirst + 1);
		d->ui.loopEnd->setValue(frameRangeLast + 1);
	}
	d->state.lastCanvasFrameRangeFirst = frameRangeFirst;
	d->state.lastCanvasFrameRangeLast = frameRangeLast;
	updateRange();

	if(refresh) {
		renderFrames();
	} else {
		QSize canvasSize = d->canvasState.size();
		QPoint canvasOffset = d->canvasState.offset();
		d->crop = QRect{QPoint{0, 0}, canvasSize};
		if(!crop.isEmpty()) {
			qreal w = canvasSize.width();
			qreal h = canvasSize.height();
			setCrop(QRectF(
				crop.x() / w, crop.y() / h, crop.width() / w,
				crop.height() / h));
		} else if(
			d->state.crop.isValid() && canvasSize == d->state.lastCanvasSize &&
			canvasOffset == d->state.lastCanvasOffset) {
			setCrop(d->state.crop);
		} else {
			resetCrop();
		}
	}
}

int Flipbook::getTimerInterval() const
{
	double framerate =
		d->canvasState.isNull() ? 24.0 : d->canvasState.effectiveFramerate();
	double speedPercent = d->ui.speedSpinner->value();
	return qRound((1000.0 / framerate) / (speedPercent / 100.0));
}

void Flipbook::renderFrames()
{
	d->frames.clear();
	d->batchId = d->animationRenderer->render(
		d->canvasState, d->crop,
		compat::widgetScreen(*this)->availableSize() * 0.9,
		d->ui.loopStart->value() - 1, d->ui.loopEnd->value(),
		d->ui.layerIndex->value() - 1);
}

void Flipbook::insertRenderedFrames(
	unsigned int batchId, const QVector<int> &frameIndexes,
	const QPixmap &frame)
{
	if(batchId == d->batchId) {
		int current = d->ui.layerIndex->value() - 1;
		bool containsCurrent = false;
		for(int i : frameIndexes) {
			d->frames[i] = frame;
			if(i == current) {
				containsCurrent = true;
			}
		}
		if(containsCurrent) {
			loadFrame();
		}
	}
}

void Flipbook::nextFrame()
{
	if(d->stalled) {
		d->skipped = true;
	} else {
		d->skipped = false;
		d->ui.layerIndex->stepUp();
	}
}

void Flipbook::loadFrame()
{
	int i = d->ui.layerIndex->value() - 1;
	if(d->frames.contains(i)) {
		d->stalled = false;
		d->ui.view->setLoading(false);
		d->ui.view->setPixmap(d->frames[i]);
		if(d->skipped) {
			nextFrame();
		}
	} else {
		d->stalled = true;
		d->ui.view->setLoading(true);
	}
}

}
