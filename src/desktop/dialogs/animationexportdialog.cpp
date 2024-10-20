// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/animationexportdialog.h"
#include "desktop/filewrangler.h"
#include "desktop/main.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/documentmetadata.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/export/animationformat.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPair>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace dialogs {

AnimationExportDialog::AnimationExportDialog(
	int scalePercent, bool scaleSmooth, QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Export Animation"));
	resize(400, 300);

	QVBoxLayout *layout = new QVBoxLayout(this);

	QTabWidget *tabs = new QTabWidget;
	layout->addWidget(tabs);

	QWidget *outputWidget = new QWidget;
	QFormLayout *outputForm = new QFormLayout(outputWidget);
	tabs->addTab(outputWidget, tr("Output"));

	m_formatCombo = new QComboBox;
	QPair<QString, AnimationFormat> formats[] = {
		{tr("Frames as PNGs"), AnimationFormat::Frames},
		{tr("Frames as PNGs in ZIP"), AnimationFormat::Zip},
		{tr("Animated GIF"), AnimationFormat::Gif},
		{tr("Animated WEBP"), AnimationFormat::Webp},
		{tr("MP4 Video"), AnimationFormat::Mp4},
		{tr("WEBM Video"), AnimationFormat::Webm},
	};
	int lastFormat = dpApp().settings().animationExportFormat();
	for(const QPair<QString, AnimationFormat> &p : formats) {
		if(isAnimationFormatSupported(p.second)) {
			int format = int(p.second);
			m_formatCombo->addItem(p.first, format);
			if(format == lastFormat) {
				m_formatCombo->setCurrentIndex(m_formatCombo->count() - 1);
			}
		}
	}
	outputForm->addRow(tr("Format:"), m_formatCombo);

	m_loopsLabel = new QLabel(tr("Loops:"));
	m_loopsSpinner = new QSpinBox;
	m_loopsSpinner->setRange(1, 99);
	outputForm->addRow(m_loopsLabel, m_loopsSpinner);

	m_paletteLabel = new QLabel(tr("Palette:"));
	m_paletteCombo = new QComboBox;
	m_paletteCombo->addItem(tr("Generate from normal view"), 1);
	m_paletteCombo->addItem(tr("Generate from frames"), 0);
	outputForm->addRow(m_paletteLabel, m_paletteCombo);

	m_scaleSpinner = new QSpinBox;
	m_scaleSpinner->setRange(1, 1000);
	m_scaleSpinner->setValue(scalePercent);
	m_scaleSpinner->setSuffix(tr("%"));

	m_scaleSmoothBox = new QCheckBox(tr("Smooth"));
	m_scaleSmoothBox->setChecked(scaleSmooth);

	QHBoxLayout *scaleLayout = new QHBoxLayout;
	scaleLayout->addWidget(m_scaleSpinner);
	scaleLayout->addWidget(m_scaleSmoothBox);
	outputForm->addRow(tr("Scaling:"), scaleLayout);

	m_scaleLabel = new QLabel;
	outputForm->addRow(m_scaleLabel);

	QWidget *inputWidget = new QWidget;
	QFormLayout *inputForm = new QFormLayout(inputWidget);
	tabs->addTab(inputWidget, tr("Input"));

	QHBoxLayout *rangeLayout = new QHBoxLayout;
	inputForm->addRow(tr("Frame Range:"), rangeLayout);

	m_startSpinner = new QSpinBox;
	rangeLayout->addWidget(m_startSpinner, 1);

	rangeLayout->addWidget(new QLabel(QStringLiteral("-")));

	m_endSpinner = new QSpinBox;
	rangeLayout->addWidget(m_endSpinner, 1);

	m_framerateSpinner = new QSpinBox;
	m_framerateSpinner->setRange(1, 999);
	m_framerateSpinner->setSuffix(tr(" FPS"));
	inputForm->addRow(tr("Framerate:"), m_framerateSpinner);

	QHBoxLayout *xCropLayout = new QHBoxLayout;
	inputForm->addRow(tr("Crop X:"), xCropLayout);

	m_x1Spinner = new QSpinBox;
	xCropLayout->addWidget(m_x1Spinner, 1);

	xCropLayout->addWidget(new QLabel(QStringLiteral("-")));

	m_x2Spinner = new QSpinBox;
	xCropLayout->addWidget(m_x2Spinner, 1);

	QHBoxLayout *yCropLayout = new QHBoxLayout;
	inputForm->addRow(tr("Crop Y:"), yCropLayout);

	m_y1Spinner = new QSpinBox;
	yCropLayout->addWidget(m_y1Spinner, 1);

	yCropLayout->addWidget(new QLabel(QStringLiteral("-")));

	m_y2Spinner = new QSpinBox;
	yCropLayout->addWidget(m_y2Spinner, 1);

	QHBoxLayout *inputButtonsLayout = new QHBoxLayout;
	inputForm->addRow(inputButtonsLayout);

	m_inputResetButton =
		new QPushButton(QIcon::fromTheme("view-refresh"), tr("Reset"));
	m_inputResetButton->setToolTip(
		tr("Reset inputs to the whole canvas and timeline"));
	inputButtonsLayout->addWidget(m_inputResetButton);

	m_inputToFlipbookButton = new QPushButton(
		QIcon::fromTheme("media-playback-start"), tr("Set from Flipbook"));
	m_inputToFlipbookButton->setToolTip(
		tr("Set inputs to what is being previewed in the flipbook"));
	m_inputToFlipbookButton->setEnabled(false);
	inputButtonsLayout->addWidget(m_inputToFlipbookButton);

	m_buttons =
		new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	m_buttons->button(QDialogButtonBox::Ok)->setText(tr("Export"));
	layout->addWidget(m_buttons);

	connect(
		m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &AnimationExportDialog::updateOutputUi);
	connect(
		m_scaleSpinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&AnimationExportDialog::updateScalingUi);
	connect(
		m_startSpinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&AnimationExportDialog::updateEndRange);
	connect(
		m_endSpinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&AnimationExportDialog::updateStartRange);
	connect(
		m_x1Spinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&AnimationExportDialog::updateX2Range);
	connect(
		m_x1Spinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&AnimationExportDialog::updateScalingUi);
	connect(
		m_x2Spinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&AnimationExportDialog::updateX1Range);
	connect(
		m_x2Spinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&AnimationExportDialog::updateScalingUi);
	connect(
		m_y1Spinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&AnimationExportDialog::updateY2Range);
	connect(
		m_y1Spinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&AnimationExportDialog::updateScalingUi);
	connect(
		m_y2Spinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&AnimationExportDialog::updateY1Range);
	connect(
		m_y2Spinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&AnimationExportDialog::updateScalingUi);
	connect(
		m_inputResetButton, &QPushButton::clicked, this,
		&AnimationExportDialog::resetInputs);
	connect(
		m_inputToFlipbookButton, &QPushButton::clicked, this,
		&AnimationExportDialog::setInputsFromFlipbook);
	connect(
		m_buttons, &QDialogButtonBox::accepted, this,
		&AnimationExportDialog::accept);
	connect(
		m_buttons, &QDialogButtonBox::rejected, this,
		&AnimationExportDialog::reject);
	connect(
		this, &AnimationExportDialog::accepted, this,
		&AnimationExportDialog::requestExport, Qt::DirectConnection);

	updateOutputUi();
	updateScalingUi();
}

void AnimationExportDialog::setCanvas(canvas::CanvasModel *canvas)
{
	if(canvas) {
		connect(
			canvas->paintEngine(), &canvas::PaintEngine::resized, this,
			&AnimationExportDialog::setCanvasSize);
		canvas::DocumentMetadata *metadata = canvas->metadata();
		connect(
			metadata, &canvas::DocumentMetadata::framerateChanged, this,
			&AnimationExportDialog::setCanvasFramerate);
		connect(
			metadata, &canvas::DocumentMetadata::frameCountChanged, this,
			&AnimationExportDialog::setCanvasFrameCount);
		setCanvasSize(canvas->size());
		setCanvasFramerate(metadata->framerate());
		setCanvasFrameCount(metadata->frameCount());
	}
}

void AnimationExportDialog::setFlipbookState(
	int start, int end, double speedPercent, const QRectF &crop, bool apply)
{
	m_flipbookStart = start;
	m_flipbookEnd = end;
	m_flipbookFramerate = qRound(m_canvasFramerate * speedPercent / 100.0);
	m_flipbookCrop = QRect(
		crop.x() * m_canvasWidth, crop.y() * m_canvasHeight,
		crop.width() * m_canvasWidth, crop.height() * m_canvasHeight);
	bool valid =
		start >= 0 || end >= 0 || speedPercent > 0.0 || !crop.isEmpty();
	m_inputToFlipbookButton->setEnabled(valid);
	if(valid && apply) {
		setInputsFromFlipbook();
	}
}

QSize AnimationExportDialog::getScaledSizeFor(
	int scalePercent, const QRect &rect)
{
	QSize size = rect.size() * (scalePercent / 100.0);
	return QSize(qMax(size.width(), 1), qMax(size.height(), 1));
}

#ifndef __EMSCRIPTEN__
void AnimationExportDialog::accept()
{
	m_path = choosePath();
	if(!m_path.isEmpty()) {
		QDialog::accept();
	}
}
#endif

void AnimationExportDialog::updateOutputUi()
{
	int format = m_formatCombo->currentData().toInt();
	bool showLoops = format == int(AnimationFormat::Mp4) ||
					 format == int(AnimationFormat::Webm);
	m_loopsLabel->setVisible(showLoops);
	m_loopsSpinner->setVisible(showLoops);
	bool showPalette = format == int(AnimationFormat::Gif);
	m_paletteLabel->setVisible(showPalette);
	m_paletteCombo->setVisible(showPalette);
}

void AnimationExportDialog::updateScalingUi()
{
	int scalePercent = m_scaleSpinner->value();
	QSize size = getScaledSizeFor(scalePercent, getCropRect());
	m_scaleLabel->setText(tr("Output resolution will be %1x%2 pixels.")
							  .arg(size.width())
							  .arg(size.height()));
}

#ifndef __EMSCRIPTEN__
QString AnimationExportDialog::choosePath()
{
	AnimationFormat format =
		AnimationFormat(m_formatCombo->currentData().toInt());
	switch(format) {
	case AnimationFormat::Frames:
		return FileWrangler(this).getSaveAnimationFramesPath();
	case AnimationFormat::Gif:
		return FileWrangler(this).getSaveAnimationGifPath();
	case AnimationFormat::Zip:
		return FileWrangler(this).getSaveAnimationZipPath();
	case AnimationFormat::Webp:
		return FileWrangler(this).getSaveAnimationWebpPath();
	case AnimationFormat::Mp4:
		return FileWrangler(this).getSaveAnimationMp4Path();
	case AnimationFormat::Webm:
		return FileWrangler(this).getSaveAnimationWebmPath();
	}
	qWarning("choosePath: unhandled format %d", int(format));
	return QString();
}
#endif

void AnimationExportDialog::updateStartRange(int end)
{
	m_startSpinner->setRange(1, end);
}

void AnimationExportDialog::updateEndRange(int start)
{
	m_endSpinner->setRange(start, m_canvasFrameCount);
}

void AnimationExportDialog::updateX1Range(int x2)
{
	m_x1Spinner->setRange(0, x2);
}

void AnimationExportDialog::updateX2Range(int x1)
{
	m_x2Spinner->setRange(x1, m_canvasWidth - 1);
}

void AnimationExportDialog::updateY1Range(int y2)
{
	m_y1Spinner->setRange(0, y2);
}

void AnimationExportDialog::updateY2Range(int y1)
{
	m_y2Spinner->setRange(y1, m_canvasHeight - 1);
}

void AnimationExportDialog::resetInputs()
{
	QSignalBlocker x1Blocker(m_x1Spinner);
	QSignalBlocker x2Blocker(m_x2Spinner);
	QSignalBlocker y1Blocker(m_y1Spinner);
	QSignalBlocker y2Blocker(m_y2Spinner);
	QSignalBlocker startBlocker(m_startSpinner);
	QSignalBlocker endBlocker(m_endSpinner);
	m_x1Spinner->setValue(0);
	m_x2Spinner->setValue(m_canvasWidth - 1);
	m_y1Spinner->setValue(0);
	m_y2Spinner->setValue(m_canvasHeight - 1);
	m_framerateSpinner->setValue(m_canvasFramerate);
	m_startSpinner->setValue(1);
	m_endSpinner->setValue(m_canvasFrameCount);
}

void AnimationExportDialog::setInputsFromFlipbook()
{
	resetInputs();
	if(m_flipbookStart > 0) {
		m_startSpinner->setValue(m_flipbookStart);
	}
	if(m_flipbookEnd > 0) {
		m_endSpinner->setValue(m_flipbookEnd);
	}
	if(m_flipbookFramerate > 0) {
		m_framerateSpinner->setValue(m_flipbookFramerate);
	}
	if(!m_flipbookCrop.isEmpty()) {
		m_x1Spinner->setValue(m_flipbookCrop.left());
		m_x2Spinner->setValue(m_flipbookCrop.right());
		m_y1Spinner->setValue(m_flipbookCrop.top());
		m_y2Spinner->setValue(m_flipbookCrop.bottom());
	}
}

void AnimationExportDialog::setCanvasSize(const QSize &size)
{
	QSignalBlocker x1Blocker(m_x1Spinner);
	QSignalBlocker x2Blocker(m_x2Spinner);
	QSignalBlocker y1Blocker(m_y1Spinner);
	QSignalBlocker y2Blocker(m_y2Spinner);
	int right = size.width() - 1;
	int bottom = size.height() - 1;
	if(m_canvasWidth < 0 || m_canvasHeight < 0) {
		m_x1Spinner->setRange(0, right);
		m_x1Spinner->setValue(0);
		m_x2Spinner->setRange(0, right);
		m_x2Spinner->setValue(right);
		m_y1Spinner->setRange(0, bottom);
		m_y1Spinner->setValue(0);
		m_y2Spinner->setRange(0, bottom);
		m_y2Spinner->setValue(bottom);
	} else {
		int prevX2 = m_x2Spinner->value();
		int prevY2 = m_y2Spinner->value();
		int x2 = right < 0 ? 0 : qBound(0, prevX2, right);
		int y2 = bottom < 0 ? 0 : qBound(0, prevY2, bottom);
		m_x1Spinner->setRange(0, x2);
		m_x2Spinner->setRange(m_x1Spinner->value(), right);
		m_y1Spinner->setRange(0, y2);
		m_y2Spinner->setRange(m_y1Spinner->value(), bottom);
	}
	m_canvasWidth = size.width();
	m_canvasHeight = size.height();
	updateScalingUi();
}

void AnimationExportDialog::setCanvasFrameCount(int frameCount)
{
	QSignalBlocker startBlocker(m_startSpinner);
	QSignalBlocker endBlocker(m_endSpinner);
	if(m_canvasFrameCount < 0) {
		m_startSpinner->setRange(1, frameCount);
		m_startSpinner->setValue(1);
		m_endSpinner->setRange(1, frameCount);
		m_endSpinner->setValue(frameCount);
	} else {
		int end = qBound(1, m_endSpinner->value(), frameCount);
		m_startSpinner->setRange(1, end);
		m_endSpinner->setRange(m_startSpinner->value(), frameCount);
	}
	m_canvasFrameCount = frameCount;
}

void AnimationExportDialog::setCanvasFramerate(int framerate)
{
	if(m_canvasFramerate < 0) {
		m_framerateSpinner->setValue(framerate);
	}
	m_canvasFramerate = framerate;
}

void AnimationExportDialog::requestExport()
{
	int format = m_formatCombo->currentData().toInt();
	dpApp().settings().setAnimationExportFormat(format);
	emit exportRequested(
#ifndef __EMSCRIPTEN__
		m_path,
#endif
		format, m_loopsSpinner->value(), m_startSpinner->value() - 1,
		m_endSpinner->value() - 1, m_framerateSpinner->value(), getCropRect(),
		m_scaleSpinner->value(), m_scaleSmoothBox->isChecked(),
		m_paletteCombo->currentData().toInt() == 1);
}

QRect AnimationExportDialog::getCropRect() const
{
	return QRect(
		QPoint(m_x1Spinner->value(), m_y1Spinner->value()),
		QPoint(m_x2Spinner->value(), m_y2Spinner->value()));
}

}
