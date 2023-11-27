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
#include "libclient/drawdance/viewmode.h"
#include "libshared/util/qtcompat.h"
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

	QMenu *exportMenu = new QMenu{this};
	QAction *exportGifAction = exportMenu->addAction(tr("Export &GIF…"));
#ifndef Q_OS_ANDROID
	QAction *exportFramesAction = exportMenu->addAction(tr("Export &Frames…"));
#endif
	d->ui.exportButton->setMenu(exportMenu);
	d->ui.exportButton->setPopupMode(QToolButton::InstantPopup);

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
		d->ui.speedSpinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&Flipbook::updateSpeed);
	connect(&d->timer, &QTimer::timeout, this, &Flipbook::nextFrame);
	connect(d->ui.view, &FlipbookView::cropped, this, &Flipbook::setCrop);
	connect(
		d->ui.zoomButton, &QToolButton::clicked, this, &Flipbook::resetCrop);
	connect(
		d->ui.refreshButton, &QToolButton::clicked, this,
		&Flipbook::refreshCanvas);
	connect(exportGifAction, &QAction::triggered, this, &Flipbook::exportGif);
#ifndef Q_OS_ANDROID
	connect(
		exportFramesAction, &QAction::triggered, this, &Flipbook::exportFrames);
#endif
	connect(
		d->ui.buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	d->ui.speedSpinner->setExponentRatio(3.0);
	d->ui.playButton->setFocus();

	utils::setGeometryIfOnScreen(this, dpApp().settings().flipbookWindow());
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
		dpApp().settings().setFlipbookWindow(geometry());
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
	d->ui.layerIndex->setMinimum(loopStart);
	d->ui.layerIndex->setMaximum(loopEnd);
	d->state.loopStart = loopStart;
	d->state.loopEnd = loopEnd;
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
	int speedPercent = d->ui.speedSpinner->value();
	d->state.speedPercent = speedPercent;
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
		qreal fps =
			d->canvasState.framerate() * d->ui.speedSpinner->value() / 100.0;
		fpsSuffix = tr("% (%1 FPS)").arg(qRound(fps));
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
		d->ui.zoomButton->setEnabled(false);
		d->ui.zoomButton->setVisible(false);
	} else {
		d->crop = QRect(
					  d->crop.x() + rect.x() * w, d->crop.y() + rect.y() * h,
					  rect.width() * w, rect.height() * h)
					  .intersected(canvasRect);
		d->ui.zoomButton->setEnabled(true);
		d->ui.zoomButton->setVisible(true);
	}

	d->state.crop = rect;
	d->state.lastCanvasOffset =
		d->canvasState.isNull() ? QPoint{} : d->canvasState.offset();
	d->state.lastCanvasSize = canvasSize;

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

void Flipbook::exportGif()
{
	if(!d->canvasState.isNull()) {
		emit exportGifRequested(
			d->canvasState, getExportRect(), getExportStart(), getExportEnd(),
			getExportFramerate());
	}
}

#ifndef Q_OS_ANDROID
void Flipbook::exportFrames()
{
	if(!d->canvasState.isNull()) {
		emit exportFramesRequested(
			d->canvasState, getExportRect(), getExportStart(), getExportEnd());
	}
}
#endif

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

	int frameCount = d->canvasState.frameCount();
	d->ui.layerIndex->setSuffix(QStringLiteral("/%1").arg(frameCount));

	if(d->state.speedPercent > 0.0) {
		d->ui.speedSpinner->setValue(d->state.speedPercent);
	} else {
		d->ui.speedSpinner->setValue(100.0);
	}

	QSignalBlocker loopStartBlocker{d->ui.loopStart};
	QSignalBlocker loopEndBlocker{d->ui.loopEnd};
	d->ui.loopStart->setMaximum(frameCount);
	d->ui.loopEnd->setMaximum(frameCount);
	if(d->state.loopStart > 0 && d->state.loopEnd > 0) {
		d->ui.loopStart->setValue(d->state.loopStart);
		d->ui.loopEnd->setValue(
			d->ui.loopEnd->value() < d->state.lastCanvasFrameCount
				? d->state.loopEnd
				: frameCount);
	} else {
		d->ui.loopStart->setValue(1);
		d->ui.loopEnd->setValue(frameCount);
	}
	d->state.lastCanvasFrameCount = frameCount;
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
	int framerate = d->canvasState.isNull() ? 24 : d->canvasState.framerate();
	int speedPercent = d->ui.speedSpinner->value();
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

QRect Flipbook::getExportRect() const
{
	Q_ASSERT(!d->canvasState.isNull());
	if(d->crop.isValid()) {
		QRect canvasRect = QRect{QPoint{0, 0}, d->canvasState.size()};
		QRect exportRect = d->crop.intersected(canvasRect);
		if(exportRect != canvasRect) {
			return exportRect;
		}
	}
	return QRect{};
}

int Flipbook::getExportStart() const
{
	int start = d->ui.loopStart->value() - 1;
	int end = d->ui.loopEnd->value() - 1;
	return start < 0 || start > end ? 0 : start;
}

int Flipbook::getExportEnd() const
{
	Q_ASSERT(!d->canvasState.isNull());
	int start = d->ui.loopStart->value() - 1;
	int end = d->ui.loopEnd->value() - 1;
	int canvasFrameCount = d->canvasState.frameCount();
	return end >= canvasFrameCount || start > end ? canvasFrameCount - 1 : end;
}

int Flipbook::getExportFramerate() const
{
	int canvasFramerate = d->canvasState.framerate();
	double speed = d->ui.speedSpinner->value() / 100.0;
	return qMax(1, qRound(canvasFramerate * speed));
}
}
