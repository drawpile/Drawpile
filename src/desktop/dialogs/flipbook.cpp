// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpengine/view_mode.h>
}

#include "desktop/dialogs/flipbook.h"
#include "desktop/main.h"
#include "desktop/utils/qtguicompat.h"
#include "libclient/canvas/paintengine.h"

#include "ui_flipbook.h"

#include <QApplication>
#include <QEvent>
#include <QRect>
#include <QScreen>
#include <QTimer>

namespace dialogs {

Flipbook::Flipbook(QWidget *parent)
	: QDialog{parent}
	, m_ui{new Ui_Flipbook}
	, m_paintengine{nullptr}
	, m_canvasState{}
	, m_vmb{}
{
	m_ui->setupUi(this);

	m_timer = new QTimer(this);

	auto &settings = dpApp().settings();
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
		m_ui->fps, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&Flipbook::updateFps);
	connect(m_timer, &QTimer::timeout, m_ui->layerIndex, &QSpinBox::stepUp);
	connect(m_ui->view, &FlipbookView::cropped, this, &Flipbook::setCrop);
	connect(
		m_ui->zoomButton, &QToolButton::clicked, this, &Flipbook::resetCrop);

	updateRange();

	m_ui->playButton->setFocus();

	const auto geom = settings.flipbookWindow();
	if(geom.isValid()) {
		setGeometry(geom);
	}

	// Autoplay
	m_ui->playButton->click();
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
	m_ui->layerIndex->setMinimum(m_ui->loopStart->value());
	m_ui->layerIndex->setMaximum(m_ui->loopEnd->value());
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

	} else {
		m_timer->start(1000 / m_ui->fps->value());
		m_ui->playButton->setIcon(QIcon::fromTheme("media-playback-pause"));
	}
}

void Flipbook::updateFps(int newFps)
{
	if(m_timer->isActive()) {
		m_timer->setInterval(1000 / newFps);
	}
}

void Flipbook::setPaintEngine(canvas::PaintEngine *pe)
{
	m_paintengine = pe;
	m_canvasState = pe->viewCanvasState();

	const int max = m_canvasState.frameCount();
	m_ui->loopStart->setMaximum(max);
	m_ui->loopEnd->setMaximum(max);
	m_ui->layerIndex->setMaximum(max);
	m_ui->layerIndex->setSuffix(QStringLiteral("/%1").arg(max));
	m_ui->loopEnd->setValue(max);

	m_crop = QRect(QPoint(), m_canvasState.size());

	const auto crop = dpApp().settings().flipbookCrop();
	if(m_crop.contains(crop, true)) {
		m_crop = crop;
		m_ui->zoomButton->setEnabled(true);
	} else {
		m_ui->zoomButton->setEnabled(false);
	}
	dpApp().settings().setFlipbookCrop(m_crop);

	drawdance::DocumentMetadata documentMetadata =
		m_canvasState.documentMetadata();
	m_realFps = documentMetadata.framerate();

	m_ui->timelineModeLabel->setText(tr("Actual FPS: %1").arg(m_realFps));
	m_ui->fps->setValue(m_realFps);

	updateFps(m_ui->fps->value());
	resetFrameCache();
	loadFrame();
}

void Flipbook::setCrop(const QRectF &rect)
{
	const int w = m_crop.width();
	const int h = m_crop.height();

	if(rect.width() * w <= 5 || rect.height() * h <= 5) {
		m_crop = QRect{
			QPoint(), m_canvasState.isNull() ? QSize{} : m_canvasState.size()};
		m_ui->zoomButton->setEnabled(false);
	} else {
		m_crop = QRect(
			m_crop.x() + rect.x() * w, m_crop.y() + rect.y() * h,
			rect.width() * w, rect.height() * h);
		m_ui->zoomButton->setEnabled(true);
	}
	dpApp().settings().setFlipbookCrop(m_crop);
	resetFrameCache();
	loadFrame();
}

void Flipbook::resetCrop()
{
	setCrop(QRectF());
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

}
