// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpengine/view_mode.h>
}
#include "desktop/dialogs/flipbook.h"
#include "desktop/main.h"
#include "desktop/utils/qtguicompat.h"
#include "libclient/canvas/paintengine.h"
#include "ui_flipbook.h"
#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QMenu>
#include <QRect>
#include <QScreen>
#include <QSignalBlocker>
#include <QTimer>

namespace dialogs {

Flipbook::Flipbook(State &state, QWidget *parent)
	: QDialog{parent}
	, m_state{state}
	, m_ui{new Ui_Flipbook}
	, m_paintengine{nullptr}
	, m_canvasState{}
	, m_vmb{}
{
	m_ui->setupUi(this);

	m_timer = new QTimer(this);

	QMenu *exportMenu = new QMenu{this};
	QAction *exportGifAction = exportMenu->addAction(tr("Export &GIF…"));
#ifndef Q_OS_ANDROID
	QAction *exportFramesAction = exportMenu->addAction(tr("Export &Frames…"));
#endif
	m_ui->exportButton->setMenu(exportMenu);
	m_ui->exportButton->setPopupMode(QToolButton::InstantPopup);

	connect(m_ui->rewindButton, &QToolButton::clicked, this, &Flipbook::rewind);
	connect(
		m_ui->playButton, &QToolButton::clicked, this, &Flipbook::playPause);
	connect(
		m_ui->layerIndex, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&Flipbook::loadFrame);
	connect(
		m_ui->loopStart, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&Flipbook::updateRange);
	connect(
		m_ui->loopEnd, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&Flipbook::updateRange);
	connect(
		m_ui->speedSpinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&Flipbook::updateSpeed);
	connect(m_timer, &QTimer::timeout, m_ui->layerIndex, &QSpinBox::stepUp);
	connect(m_ui->view, &FlipbookView::cropped, this, &Flipbook::setCrop);
	connect(
		m_ui->zoomButton, &QToolButton::clicked, this, &Flipbook::resetCrop);
	connect(
		m_ui->refreshButton, &QToolButton::clicked, this,
		&Flipbook::refreshCanvas);
	connect(exportGifAction, &QAction::triggered, this, &Flipbook::exportGif);
#ifndef Q_OS_ANDROID
	connect(
		exportFramesAction, &QAction::triggered, this, &Flipbook::exportFrames);
#endif

	m_ui->speedSpinner->setExponentRatio(3.0);
	m_ui->playButton->setFocus();

	const auto geom = dpApp().settings().flipbookWindow();
	if(geom.isValid()) {
		setGeometry(geom);
	}
}

Flipbook::~Flipbook()
{
	delete m_ui;
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
	int loopStart = m_ui->loopStart->value();
	int loopEnd = m_ui->loopEnd->value();
	m_ui->layerIndex->setMinimum(loopStart);
	m_ui->layerIndex->setMaximum(loopEnd);
	m_state.loopStart = loopStart;
	m_state.loopEnd = loopEnd;
}

void Flipbook::rewind()
{
	m_ui->layerIndex->setValue(m_ui->layerIndex->minimum());
}

void Flipbook::playPause()
{
	if(m_timer->isActive()) {
		m_timer->stop();
		m_ui->playButton->setIcon(QIcon::fromTheme("media-playback-start"));
	} else if(!m_canvasState.isNull()) {
		m_timer->start(getTimerInterval());
		m_ui->playButton->setIcon(QIcon::fromTheme("media-playback-pause"));
	}
}

void Flipbook::updateSpeed()
{
	m_state.speedPercent = m_ui->speedSpinner->value();
	if(m_timer->isActive()) {
		m_timer->setInterval(getTimerInterval());
	}
}

void Flipbook::setPaintEngine(canvas::PaintEngine *pe)
{
	m_paintengine = pe;
	resetCanvas(false);
	if(!m_timer->isActive()) {
		playPause();
	}
}

void Flipbook::setCrop(const QRectF &rect)
{
	const int w = m_crop.width();
	const int h = m_crop.height();
	QSize canvasSize = m_canvasState.isNull() ? QSize{} : m_canvasState.size();

	if(rect.width() * w <= 5 || rect.height() * h <= 5) {
		m_crop = QRect{QPoint(), canvasSize};
		m_ui->zoomButton->setEnabled(false);
	} else {
		m_crop = QRect(
			m_crop.x() + rect.x() * w, m_crop.y() + rect.y() * h,
			rect.width() * w, rect.height() * h);
		m_ui->zoomButton->setEnabled(true);
	}

	m_state.crop = rect;
	m_state.lastCanvasOffset =
		m_canvasState.isNull() ? QPoint{} : m_canvasState.offset();
	m_state.lastCanvasSize = canvasSize;

	resetFrameCache();
	loadFrame();
}

void Flipbook::resetCrop()
{
	setCrop(QRectF());
}

void Flipbook::refreshCanvas()
{
	resetCanvas(true);
}

void Flipbook::exportGif()
{
	if(!m_canvasState.isNull()) {
		emit exportGifRequested(
			m_canvasState, getExportRect(), getExportStart(), getExportEnd(),
			getExportFramerate());
	}
}

#ifndef Q_OS_ANDROID
void Flipbook::exportFrames()
{
	if(!m_canvasState.isNull()) {
		emit exportFramesRequested(
			m_canvasState, getExportRect(), getExportStart(), getExportEnd());
	}
}
#endif

void Flipbook::resetCanvas(bool refresh)
{
	if(!m_paintengine) {
		return;
	}

	m_canvasState = m_paintengine->viewCanvasState();
	if(m_canvasState.isNull()) {
		return;
	}

	int frameCount = m_canvasState.frameCount();
	m_ui->layerIndex->setSuffix(QStringLiteral("/%1").arg(frameCount));

	if(m_state.speedPercent > 0.0) {
		m_ui->speedSpinner->setValue(m_state.speedPercent);
	} else {
		m_ui->speedSpinner->setValue(100.0);
	}

	QSignalBlocker blocker{m_ui->loopStart};
	if(m_state.loopStart > 0 && m_state.loopEnd > 0) {
		m_ui->loopStart->setValue(m_state.loopStart);
		m_ui->loopEnd->setValue(m_state.loopEnd);
	} else {
		m_ui->loopStart->setValue(1);
		m_ui->loopEnd->setValue(frameCount);
	}

	if(refresh) {
		resetFrameCache();
		loadFrame();
	} else {
		QSize canvasSize = m_canvasState.size();
		QPoint canvasOffset = m_canvasState.offset();
		m_crop = QRect{QPoint{0, 0}, canvasSize};
		if(m_state.crop.isValid() && canvasSize == m_state.lastCanvasSize &&
		   canvasOffset == m_state.lastCanvasOffset) {
			setCrop(m_state.crop);
		} else {
			resetCrop();
		}
	}
}

int Flipbook::getTimerInterval() const
{
	int framerate = m_canvasState.isNull() ? 24 : m_canvasState.framerate();
	int speedPercent = m_ui->speedSpinner->value();
	return qRound((1000.0 / framerate) / (speedPercent / 100.0));
}

void Flipbook::resetFrameCache()
{
	m_frames.clear();
	if(!m_canvasState.isNull()) {
		const int frames = m_canvasState.frameCount();
		for(int i = 0; i < frames; ++i) {
			m_frames.append(QPixmap{});
		}
	}
}

void Flipbook::loadFrame()
{
	const int f = m_ui->layerIndex->value() - 1;
	if(!m_canvasState.isNull() && f >= 0 && f < m_frames.size()) {
		if(m_frames.at(f).isNull()) {
			QPixmap frame;
			if(!m_crop.isEmpty() && !searchIdenticalFrame(f, frame)) {
				DP_ViewModeFilter vmf = DP_view_mode_filter_make_frame(
					m_vmb.get(), m_canvasState.get(), f, nullptr);
				QImage img =
					m_canvasState.toFlatImage(true, true, &m_crop, &vmf);

				// Scale down the image if it is too big
				QSize maxSize =
					compat::widgetScreen(*this)->availableSize() * 0.7;
				if(img.width() > maxSize.width() ||
				   img.height() > maxSize.height()) {
					const QSize newSize =
						QSize(img.width(), img.height()).boundedTo(maxSize);
					img = img.scaled(
						newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
				}

				frame = QPixmap::fromImage(img);
			}
			m_frames[f] = frame;
		}
		m_ui->view->setPixmap(m_frames.at(f));
	} else {
		m_ui->view->setPixmap(QPixmap{});
	}
}

bool Flipbook::searchIdenticalFrame(int f, QPixmap &outFrame)
{
	int frameCount = m_frames.size();
	for(int i = 0; i < frameCount; ++i) {
		if(f != i && !m_frames[i].isNull() && m_canvasState.sameFrame(f, i)) {
			outFrame = m_frames[i];
			return true;
		}
	}
	return false;
}

QRect Flipbook::getExportRect() const
{
	Q_ASSERT(!m_canvasState.isNull());
	if(m_crop.isValid()) {
		QRect canvasRect = QRect{QPoint{0, 0}, m_canvasState.size()};
		QRect exportRect = m_crop.intersected(canvasRect);
		if(exportRect != canvasRect) {
			return exportRect;
		}
	}
	return QRect{};
}

int Flipbook::getExportStart() const
{
	int start = m_ui->loopStart->value() - 1;
	int end = m_ui->loopEnd->value() - 1;
	return start < 0 || start > end ? 0 : start;
}

int Flipbook::getExportEnd() const
{
	Q_ASSERT(!m_canvasState.isNull());
	int start = m_ui->loopStart->value() - 1;
	int end = m_ui->loopEnd->value() - 1;
	int canvasFrameCount = m_canvasState.frameCount();
	return end >= canvasFrameCount || start > end ? canvasFrameCount - 1 : end;
}

int Flipbook::getExportFramerate() const
{
	int canvasFramerate = m_canvasState.framerate();
	double speed = m_ui->speedSpinner->value() / 100.0;
	return qMax(1, qRound(canvasFramerate * speed));
}

}
