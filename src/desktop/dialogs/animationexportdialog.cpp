// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/animationexportdialog.h"
#include "desktop/filewrangler.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "desktop/widgets/noscroll.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/documentmetadata.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/config/config.h"
#include "libclient/export/videoformat.h"
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
#include <QTabWidget>
#include <QVBoxLayout>
#ifdef DRAWPILE_FFMPEG_DIALOG
#	include "desktop/dialogs/ffmpegdialog.h"
#endif

namespace dialogs {

AnimationExportDialog::AnimationExportDialog(
	int loops, double scalePercent, bool scaleSmooth, QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Export Animation"));
	resize(600, 400);

	config::Config *cfg = dpAppConfig();
	QVBoxLayout *layout = new QVBoxLayout(this);

	QTabWidget *tabs = new QTabWidget;
	layout->addWidget(tabs);

	QWidget *outputWidget = new QWidget;
	QVBoxLayout *outputLayout = new QVBoxLayout(outputWidget);
	tabs->addTab(outputWidget, tr("Output"));

	QFormLayout *outputForm = new QFormLayout;
	outputLayout->addLayout(outputForm);

	bool anyFormatFfmpegSupported;
	QVector<VideoFormatOption> formatOptions = getVideoFormatOptions(
		VideoFormatApplication::Animation, &anyFormatFfmpegSupported);
	int lastFormat = cfg->getAnimationExportFormat();
	m_formatCombo = new widgets::NoScrollComboBox;
	outputForm->addRow(tr("Format:"), m_formatCombo);

	for(const VideoFormatOption &vfo : formatOptions) {
		if(vfo.nonFfmpegSupported) {
			m_formatCombo->addItem(vfo.title, int(vfo.format));
			if(int(vfo.format) == lastFormat) {
				m_formatCombo->setCurrentIndex(m_formatCombo->count() - 1);
			}
		}
	}

	if(anyFormatFfmpegSupported) {
		bool needSeparator = true;
		for(const VideoFormatOption &vfo : formatOptions) {
			if(vfo.isOnlyFfmpegSupported()) {
				if(needSeparator) {
					needSeparator = false;
					int separatorIndex = m_formatCombo->count();
					if(separatorIndex != 0) {
						m_formatCombo->insertSeparator(separatorIndex);
					}
				}

				m_formatCombo->addItem(vfo.title, int(vfo.format));
				if(int(vfo.format) == lastFormat) {
					m_formatCombo->setCurrentIndex(m_formatCombo->count() - 1);
				}
			}
		}

		m_ffmpegFormatNote = new utils::FormNote(
			QCoreApplication::translate(
				"dialogs::TimelapseDialog",
				"This format requires FFmpeg, click here to set it up."),
			false, QIcon::fromTheme(QStringLiteral("dialog-warning")), true);
		m_ffmpegFormatNote->hide();
		outputForm->addRow(nullptr, m_ffmpegFormatNote);
		connect(
			m_ffmpegFormatNote, &utils::FormNote::linkClicked, this,
			&AnimationExportDialog::showFfmpegSettings);
	}

	m_loopsLabel = new QLabel(tr("Loops:"));
	m_loopsSpinner = new KisSliderSpinBox;
	m_loopsSpinner->setIndeterminate(true);
	m_loopsSpinner->setRange(1, 99);
	m_loopsSpinner->setValue(loops);
	outputForm->addRow(m_loopsLabel, m_loopsSpinner);

	m_scaleSpinner = new KisDoubleSliderSpinBox;
	m_scaleSpinner->setIndeterminate(true);
	m_scaleSpinner->setRange(1.0, 1000.0, 2);
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

	utils::addFormSpacer(outputLayout);

	if(anyFormatFfmpegSupported) {
		QHBoxLayout *ffmpegButtonLayout = new QHBoxLayout;
		ffmpegButtonLayout->setContentsMargins(0, 0, 0, 0);
		outputLayout->addLayout(ffmpegButtonLayout);

		m_ffmpegButton = new QPushButton(
			QIcon::fromTheme(QStringLiteral("kdenlive-show-video")), QString());
		utils::setWidgetRetainSizeWhenHidden(m_ffmpegButton, true);
		m_ffmpegButton->setFlat(true);
		m_ffmpegButton->hide();
		ffmpegButtonLayout->addWidget(m_ffmpegButton);
		connect(
			m_ffmpegButton, &QPushButton::clicked, this,
			&AnimationExportDialog::showFfmpegSettings);

		ffmpegButtonLayout->addStretch();
	}

	QHBoxLayout *advancedButtonLayout = new QHBoxLayout;
	advancedButtonLayout->setContentsMargins(0, 0, 0, 0);
	outputLayout->addLayout(advancedButtonLayout);

	m_advancedButton = new QPushButton(
		QCoreApplication::translate(
			"dialogs::TimelapseDialog", "Advanced settings"));
	m_advancedButton->setFlat(true);
	advancedButtonLayout->addWidget(m_advancedButton);
	connect(
		m_advancedButton, &QPushButton::clicked, this,
		&AnimationExportDialog::toggleAdvanced);

	advancedButtonLayout->addStretch();

	m_advancedWidget = new QWidget;
	m_advancedWidget->setContentsMargins(0, 0, 0, 0);
	m_advancedWidget->setEnabled(false);
	m_advancedWidget->setVisible(false);
	outputLayout->addWidget(m_advancedWidget);

	m_advancedWidget = new QWidget;
	m_advancedWidget->setContentsMargins(0, 0, 0, 0);
	m_advancedWidget->setEnabled(false);
	m_advancedWidget->setVisible(false);
	outputLayout->addWidget(m_advancedWidget);

	QFormLayout *advancedForm = new QFormLayout(m_advancedWidget);
	advancedForm->setContentsMargins(0, 0, 0, 0);

	m_encoderCombo = new widgets::NoScrollComboBox;
	advancedForm->addRow(tr("Encoder:"), m_encoderCombo);
	connect(
		m_encoderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &AnimationExportDialog::updateEncoder);

	if(anyFormatFfmpegSupported) {
		m_ffmpegEncoderNote = new utils::FormNote(
			QCoreApplication::translate(
				"dialogs::TimelapseDialog",
				"This encoder requires FFmpeg, click here to set it up."),
			false, QIcon::fromTheme(QStringLiteral("dialog-warning")), true);
		m_ffmpegEncoderNote->hide();
		advancedForm->addRow(nullptr, m_ffmpegEncoderNote);
		connect(
			m_ffmpegEncoderNote, &utils::FormNote::linkClicked, this,
			&AnimationExportDialog::showFfmpegSettings);
	}

	outputLayout->addStretch();

	QWidget *inputWidget = new QWidget;
	QFormLayout *inputForm = new QFormLayout(inputWidget);
	tabs->addTab(inputWidget, tr("Input"));

	QHBoxLayout *rangeLayout = new QHBoxLayout;
	inputForm->addRow(tr("Frame Range:"), rangeLayout);

	m_startSpinner = new widgets::NoScrollKisSliderSpinBox;
	m_startSpinner->setIndeterminate(true);
	rangeLayout->addWidget(m_startSpinner, 1);

	rangeLayout->addWidget(new QLabel(QStringLiteral("-")));

	m_endSpinner = new widgets::NoScrollKisSliderSpinBox;
	m_endSpinner->setIndeterminate(true);
	rangeLayout->addWidget(m_endSpinner, 1);

	m_framerateSpinner = new widgets::NoScrollKisDoubleSliderSpinBox;
	m_framerateSpinner->setIndeterminate(true);
	m_framerateSpinner->setRange(0.01, 999.99, 2);
	m_framerateSpinner->setSuffix(tr(" FPS"));
	inputForm->addRow(tr("Framerate:"), m_framerateSpinner);

	QHBoxLayout *xCropLayout = new QHBoxLayout;
	inputForm->addRow(tr("Crop X:"), xCropLayout);

	m_x1Spinner = new widgets::NoScrollKisSliderSpinBox;
	m_x1Spinner->setIndeterminate(true);
	xCropLayout->addWidget(m_x1Spinner, 1);

	xCropLayout->addWidget(new QLabel(QStringLiteral("-")));

	m_x2Spinner = new widgets::NoScrollKisSliderSpinBox;
	m_x2Spinner->setIndeterminate(true);
	xCropLayout->addWidget(m_x2Spinner, 1);

	QHBoxLayout *yCropLayout = new QHBoxLayout;
	inputForm->addRow(tr("Crop Y:"), yCropLayout);

	m_y1Spinner = new widgets::NoScrollKisSliderSpinBox;
	m_y1Spinner->setIndeterminate(true);
	yCropLayout->addWidget(m_y1Spinner, 1);

	yCropLayout->addWidget(new QLabel(QStringLiteral("-")));

	m_y2Spinner = new widgets::NoScrollKisSliderSpinBox;
	m_y2Spinner->setIndeterminate(true);
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
		this, &AnimationExportDialog::updateFormat);
	connect(
		m_scaleSpinner,
		QOverload<double>::of(&KisDoubleSliderSpinBox::valueChanged), this,
		&AnimationExportDialog::updateScalingUi);
	connect(
		m_x1Spinner, QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&AnimationExportDialog::updateX2Range);
	connect(
		m_x1Spinner, QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&AnimationExportDialog::updateScalingUi);
	connect(
		m_x2Spinner, QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&AnimationExportDialog::updateX1Range);
	connect(
		m_x2Spinner, QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&AnimationExportDialog::updateScalingUi);
	connect(
		m_y1Spinner, QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&AnimationExportDialog::updateY2Range);
	connect(
		m_y1Spinner, QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&AnimationExportDialog::updateScalingUi);
	connect(
		m_y2Spinner, QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&AnimationExportDialog::updateY1Range);
	connect(
		m_y2Spinner, QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
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

	m_ffmpegPath = cfg->getFfmpegPath();
	m_preferredEncoders = cfg->getAnimationExportPreferredEncoders();
	updateFormat();
	updateScalingUi();
	updateFfmpegFormatIcons();
	updateAdvanced(cfg->getAnimationExportShowAdvanced());
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
		connect(
			metadata, &canvas::DocumentMetadata::frameRangeChanged, this,
			&AnimationExportDialog::setCanvasFrameRange);
		setCanvasSize(canvas->size());
		setCanvasFramerate(metadata->framerate());
		setCanvasFrameCount(metadata->frameCount());
		setCanvasFrameRange(
			metadata->frameRangeFirst(), metadata->frameRangeLast());
	}
}

void AnimationExportDialog::setFlipbookState(
	int start, int end, double speedPercent, const QRectF &crop, bool apply)
{
	m_flipbookStart = start;
	m_flipbookEnd = end;
	m_flipbookFramerate = m_canvasFramerate * speedPercent / 100.0;
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
	double scalePercent, const QRect &rect)
{
	QSizeF size = QSizeF(rect.size()) * (scalePercent / 100.0);
	return QSize(qMax(qRound(size.width()), 1), qMax(qRound(size.height()), 1));
}

void AnimationExportDialog::accept()
{
#ifdef __EMSCRIPTEN__
	saveSettings();
	QDialog::accept();
#else
	if(isCurrentEncoderFfmpeg() && m_ffmpegPath.isEmpty()) {
		QMessageBox *box = utils::showQuestion(
			this, tr("FFmpeg"),
			QCoreApplication::translate(
				"dialogs::TimelapseDialog",
				"The selected encoder requires FFmpeg. Do you want to set it "
				"up now?"));
		connect(
			box, &QMessageBox::accepted, this,
			&AnimationExportDialog::showFfmpegSettings);
		return;
	}

	m_path = choosePath();
	if(!m_path.isEmpty()) {
		saveSettings();
		QDialog::accept();
	}
#endif
}

void AnimationExportDialog::reject()
{
	saveSettings();
	QDialog::reject();
}

void AnimationExportDialog::updateScalingUi()
{
	double scalePercent = m_scaleSpinner->value();
	QSize size = getScaledSizeFor(scalePercent, getCropRect());
	m_scaleLabel->setText(tr("Output resolution will be %1x%2 pixels.")
							  .arg(size.width())
							  .arg(size.height()));
}

void AnimationExportDialog::toggleAdvanced()
{
	updateAdvanced(!m_advancedWidget->isEnabled());
}

void AnimationExportDialog::updateAdvanced(bool enabled)
{
	QSignalBlocker blocker(m_advancedButton);

	QString iconName;
	if(enabled) {
		iconName = QStringLiteral("arrow-down");
	} else if(isRightToLeft()) {
		iconName = QStringLiteral("arrow-left");
	} else {
		iconName = QStringLiteral("arrow-right");
	}

	m_advancedButton->setIcon(QIcon::fromTheme(iconName));
	m_advancedWidget->setEnabled(enabled);
	m_advancedWidget->setVisible(enabled);
}

void AnimationExportDialog::updateFormat()
{
	int format = m_formatCombo->currentData().toInt();
	bool showLoops = format == int(VideoFormat::Mp4Vp9) ||
					 format == int(VideoFormat::WebmVp8) ||
					 format == int(VideoFormat::Mp4H264) ||
					 format == int(VideoFormat::Mp4Av1);
	m_loopsLabel->setVisible(showLoops);
	m_loopsSpinner->setVisible(showLoops);

	QVector<VideoEncoderOption> options =
		getVideoEncoderOptions(VideoFormat(format));

	QSignalBlocker blocker(m_encoderCombo);
	m_encoderCombo->clear();

	int count = options.size();
	if(count == 0) {
		m_encoderCombo->addItem(
			QCoreApplication::translate(
				"dialogs::TimelapseDialog", "Internal"));
		m_encoderCombo->setEnabled(false);
	} else {
		int automaticIndex = getAutomaticVideoEncoderOptionIndex(
			options, !m_ffmpegPath.isEmpty());
		const VideoEncoderOption &automaticOption = options[automaticIndex];
		m_encoderCombo->addItem(
			QCoreApplication::translate(
				"dialogs::TimelapseDialog", "Automatic (%1)")
				.arg(automaticOption.title),
			QVariant(automaticOption.key));

		QString preferredKey =
			m_preferredEncoders.value(QString::number(format)).toString();
		int preferredIndex = 0;
		for(int i = 0; i < count; ++i) {
			const VideoEncoderOption &option = options[i];
			m_encoderCombo->addItem(option.title, QVariant(option.key));
			if(option.key == preferredKey) {
				preferredIndex = i + 1;
			}
		}

		m_encoderCombo->setCurrentIndex(preferredIndex);
		m_encoderCombo->setEnabled(true);
	}

	updateFfmpegUi();
}

void AnimationExportDialog::updateEncoder()
{
	if(m_encoderCombo->count() != 0) {
		QString key = QString::number(m_formatCombo->currentData().toInt());
		// The first item is the "Automatic" entry.
		if(m_encoderCombo->currentIndex() == 0) {
			m_preferredEncoders.remove(key);
		} else {
			QString preferredKey = m_encoderCombo->currentData().toString();
			m_preferredEncoders.insert(key, preferredKey);
		}
	}
	updateFfmpegUi();
}

void AnimationExportDialog::updateFfmpegUi()
{
	if(m_ffmpegFormatNote) {
		m_ffmpegFormatNote->setVisible(
			m_ffmpegPath.isEmpty() && isAllEncodersFfmpeg());
	}

	if(m_ffmpegEncoderNote) {
		m_ffmpegEncoderNote->setVisible(
			m_ffmpegPath.isEmpty() && isCurrentEncoderFfmpeg());
	}

	if(m_ffmpegButton) {
		m_ffmpegButton->setVisible(isAnyEncoderFfmpeg());
		if(m_ffmpegPath.isEmpty()) {
			m_ffmpegButton->setText(
				QCoreApplication::translate(
					"dialogs::TimelapseDialog", "Set up FFmpeg"));
		} else {
			m_ffmpegButton->setText(
				QCoreApplication::translate(
					"dialogs::TimelapseDialog", "FFmpeg settings"));
		}
	}
}

bool AnimationExportDialog::isAllEncodersFfmpeg() const
{
	int count = m_encoderCombo->count();
	if(count == 0) {
		return false;
	} else {
		for(int i = 0; i < count; ++i) {
			if(!m_encoderCombo->itemData(i).toString().startsWith(
				   QStringLiteral("ffmpeg:"))) {
				return false;
			}
		}
		return true;
	}
}

bool AnimationExportDialog::isAnyEncoderFfmpeg() const
{
	int count = m_encoderCombo->count();
	for(int i = 0; i < count; ++i) {
		if(m_encoderCombo->itemData(i).toString().startsWith(
			   QStringLiteral("ffmpeg:"))) {
			return true;
		}
	}
	return false;
}

bool AnimationExportDialog::isCurrentEncoderFfmpeg() const
{
	return currentEncoderKey().startsWith(QStringLiteral("ffmpeg:"));
}

QString AnimationExportDialog::currentEncoderKey() const
{
	if(m_encoderCombo->count() == 0) {
		return QString();
	} else {
		return m_encoderCombo->currentData().toString();
	}
}

void AnimationExportDialog::showFfmpegSettings()
{
#ifdef DRAWPILE_FFMPEG_DIALOG
	QString objectName = QStringLiteral("ffmpegdialog");
	FfmpegDialog *dlg =
		findChild<FfmpegDialog *>(objectName, Qt::FindDirectChildrenOnly);
	if(dlg) {
		dlg->activateWindow();
		dlg->raise();
	} else {
		dlg = new FfmpegDialog(this);
		dlg->setAttribute(Qt::WA_DeleteOnClose);
		dlg->setObjectName(objectName);
		connect(
			dlg, &FfmpegDialog::ffmpegPathAccepted, this,
			&AnimationExportDialog::setFfmpegPath);
		dlg->show();
	}
#else
	utils::showFfmpegUnsupportedError(this);
#endif
}

void AnimationExportDialog::setFfmpegPath(const QString &ffmpegPath)
{
	if(ffmpegPath != m_ffmpegPath) {
		m_ffmpegPath = ffmpegPath;
		updateFormat();
		updateFfmpegFormatIcons();
	}
}

void AnimationExportDialog::updateFfmpegFormatIcons()
{
	QIcon icon;
	if(m_ffmpegPath.isEmpty()) {
		icon = QIcon::fromTheme(QStringLiteral("dialog-warning"));
	}

	int count = m_formatCombo->count();
	for(int i = 0; i < count; ++i) {
		int format = m_formatCombo->itemData(i).toInt();
		if(!isVideoFormatSupportedNonFfmpeg(VideoFormat(format))) {
			m_formatCombo->setItemIcon(i, icon);
		}
	}
}

#ifndef __EMSCRIPTEN__
QString AnimationExportDialog::choosePath()
{
	VideoFormat format = VideoFormat(m_formatCombo->currentData().toInt());
	switch(format) {
	case VideoFormat::Frames:
		return FileWrangler(this).getSaveAnimationFramesPath();
	case VideoFormat::Gif:
		return FileWrangler(this).getSaveAnimationGifPath();
	case VideoFormat::Zip:
		return FileWrangler(this).getSaveAnimationZipPath();
	case VideoFormat::Webp:
		return FileWrangler(this).getSaveAnimationWebpPath();
	case VideoFormat::Apng:
		return FileWrangler(this).getSaveAnimationApngPath();
	case VideoFormat::Mp4Vp9:
	case VideoFormat::Mp4H264:
	case VideoFormat::Mp4Av1:
		return FileWrangler(this).getSaveAnimationMp4Path();
	case VideoFormat::WebmVp8:
		return FileWrangler(this).getSaveAnimationWebmPath();
	}
	qWarning("choosePath: unhandled format %d", int(format));
	return QString();
}
#endif

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
	m_startSpinner->setRange(1, m_canvasFrameCount);
	m_endSpinner->setRange(1, m_canvasFrameCount);
	m_startSpinner->setValue(m_canvasFrameRangeFirst + 1);
	m_endSpinner->setValue(m_canvasFrameRangeLast + 1);
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

void AnimationExportDialog::setCanvasFrameRange(
	int frameRangeFirst, int frameRangeLast)
{
	if(m_canvasFrameRangeFirst < 0 || m_canvasFrameRangeLast < 0) {
		m_startSpinner->setValue(frameRangeFirst + 1);
		m_endSpinner->setValue(frameRangeLast + 1);
	}
	m_canvasFrameRangeFirst = frameRangeFirst;
	m_canvasFrameRangeLast = frameRangeLast;
}

void AnimationExportDialog::setCanvasFrameCount(int frameCount)
{
	m_startSpinner->setRange(1, frameCount);
	m_endSpinner->setRange(1, frameCount);
	if(m_canvasFrameCount < 0) {
		m_startSpinner->setValue(1);
		m_endSpinner->setValue(frameCount);
	}
	m_canvasFrameCount = frameCount;
}

void AnimationExportDialog::setCanvasFramerate(double framerate)
{
	if(m_canvasFramerate < 0.0) {
		m_framerateSpinner->setValue(framerate);
	}
	m_canvasFramerate = framerate;
}

void AnimationExportDialog::saveSettings()
{
	config::Config *cfg = dpAppConfig();
	cfg->setAnimationExportFormat(m_formatCombo->currentData().toInt());
	cfg->setAnimationExportPreferredEncoders(m_preferredEncoders);
	cfg->setAnimationExportShowAdvanced(m_advancedWidget->isEnabled());
}

void AnimationExportDialog::requestExport()
{
	emit exportRequested(
#ifndef __EMSCRIPTEN__
		m_path,
#endif
		m_ffmpegPath, currentEncoderKey(), m_formatCombo->currentData().toInt(),
		m_loopsSpinner->value(), buildFrameIndexes(),
		m_framerateSpinner->value(), getCropRect(), m_scaleSpinner->value(),
		m_scaleSmoothBox->isChecked());
}

QRect AnimationExportDialog::getCropRect() const
{
	return QRect(
		QPoint(m_x1Spinner->value(), m_y1Spinner->value()),
		QPoint(m_x2Spinner->value(), m_y2Spinner->value()));
}

QVector<int> AnimationExportDialog::buildFrameIndexes() const
{
	QVector<int> frameIndexes;
	int startIndex = m_startSpinner->value() - 1;
	int endIndex = m_endSpinner->value() - 1;
	// If the start is before the end, the user wants the frames in that range.
	// If they are inverted, we go from the start to the end of the canvas frame
	// range and then start again at the beginning. This allows skipping frames
	// in the middle of the animation. This is something the flipbook can do for
	// the purpose of working on looping animations and it's a bit silly to do
	// it for export, but for the sake of consistency we support that as well.
	if(startIndex <= endIndex) {
		frameIndexes.reserve(endIndex - startIndex + 1);
		for(int i = startIndex; i <= endIndex; ++i) {
			frameIndexes.append(i);
		}
	} else {
		int frameRangeFirst =
			m_canvasFrameRangeFirst < 0 ? 0 : m_canvasFrameRangeFirst;
		int frameRangeLast = m_canvasFrameRangeLast < 0
								 ? m_canvasFrameCount - 1
								 : m_canvasFrameRangeLast;
		frameIndexes.reserve(
			qMax(0, frameRangeLast - startIndex + 1) +
			qMax(0, endIndex - frameRangeFirst + 1));
		for(int i = startIndex; i <= frameRangeLast; ++i) {
			frameIndexes.append(i);
		}
		for(int i = frameRangeFirst; i <= endIndex; ++i) {
			frameIndexes.append(i);
		}
	}
	return frameIndexes;
}

}
