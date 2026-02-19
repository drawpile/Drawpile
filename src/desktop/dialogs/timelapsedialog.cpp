// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/image.h>
#include <dpmsg/messages.h>
}
#include "desktop/dialogs/colordialog.h"
#include "desktop/dialogs/ffmpegdialog.h"
#include "desktop/dialogs/timelapsedialog.h"
#include "desktop/filewrangler.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "desktop/widgets/timelapsepreview.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/config/config.h"
#include "libclient/drawdance/documentmetadata.h"
#include "libclient/drawdance/image.h"
#include "libclient/export/timelapsesaverrunnable.h"
#include "libclient/export/videoformat.h"
#include "libshared/util/paths.h"
#include <QAction>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QThreadPool>
#include <QTimeEdit>
#include <QVBoxLayout>
#include <QtColorWidgets/ColorPreview>
#include <cmath>
#include <functional>

namespace dialogs {

TimelapseDialog::TimelapseDialog(
	canvas::PaintEngine *paintEngine, const QRect &crop, bool inFrameView,
	double flipbookSpeedPercent, int flipbookFrameRangeFirst,
	int flipbookFrameRangeLast, QWidget *parent)
	: QDialog(parent)
	, m_canvasState(paintEngine->viewCanvasState())
	, m_vmf(paintEngine->viewModeFilter(m_vmb, m_canvasState))
	, m_crop(crop)
	, m_inFrameView(inFrameView)
{
	setWindowTitle(tr("Timelapse"));
	utils::makeModal(this);
	resize(600, 800);

	QVBoxLayout *layout = new QVBoxLayout;
	setLayout(layout);

	m_scroll = new QScrollArea;
	m_scroll->setContentsMargins(0, 0, 0, 0);
	utils::KineticScroller *kineticScroller =
		utils::bindKineticScrolling(m_scroll);
	layout->addWidget(m_scroll, 1);

	QWidget *scrollWidget = new QWidget;
	m_scroll->setWidget(scrollWidget);
	m_scroll->setWidgetResizable(true);

	QVBoxLayout *scrollLayout = new QVBoxLayout(scrollWidget);

	m_timelapsePreview = new widgets::TimelapsePreview;
	m_timelapsePreview->setCanvas(m_canvasState, &m_vmf);
	m_timelapsePreview->setFixedHeight(320);
	scrollLayout->addWidget(m_timelapsePreview);

	m_settingsPage = new QWidget;
	m_progressPage = new QWidget;
	m_finishPage = new QWidget;
	m_settingsPage->hide();
	m_progressPage->hide();
	m_finishPage->hide();
	m_settingsPage->setContentsMargins(0, 0, 0, 0);
	m_progressPage->setContentsMargins(0, 0, 0, 0);
	m_finishPage->setContentsMargins(0, 0, 0, 0);
	scrollLayout->addWidget(m_settingsPage);
	scrollLayout->addWidget(m_progressPage);
	scrollLayout->addWidget(m_finishPage);

	QVBoxLayout *settingsLayout = new QVBoxLayout(m_settingsPage);
	settingsLayout->setContentsMargins(0, 0, 0, 0);

	QFormLayout *settingsForm = new QFormLayout;
	settingsForm->setContentsMargins(0, 0, 0, 0);
	settingsLayout->addLayout(settingsForm);

	struct LogoLocationDefinition {
		widgets::GroupedToolButton::GroupPosition groupPosition;
		QString iconName;
		QString toolTip;
		LogoLocation location;
	};

	LogoLocationDefinition logoLocationDefinitions[] = {
		{
			widgets::GroupedToolButton::GroupLeft,
			QStringLiteral("drawpile_close"),
			tr("None"),
			LogoLocation::None,
		},
		{
			widgets::GroupedToolButton::GroupCenter,
			QStringLiteral("drawpile_topleft"),
			tr("Top-left"),
			LogoLocation::TopLeft,
		},
		{
			widgets::GroupedToolButton::GroupCenter,
			QStringLiteral("drawpile_topright"),
			tr("Top-right"),
			LogoLocation::TopRight,
		},
		{
			widgets::GroupedToolButton::GroupCenter,
			QStringLiteral("drawpile_bottomleft"),
			tr("Bottom-left"),
			LogoLocation::BottomLeft,
		},
		{
			widgets::GroupedToolButton::GroupRight,
			QStringLiteral("drawpile_bottomright"),
			tr("Bottom-right"),
			LogoLocation::BottomRight,
		},
	};

	QHBoxLayout *logoLayout = new QHBoxLayout;
	logoLayout->setSpacing(0);
	settingsForm->addRow(tr("Logo:"), logoLayout);
	m_logoLocationGroup = new QButtonGroup(this);

	for(const LogoLocationDefinition &def : logoLocationDefinitions) {
		widgets::GroupedToolButton *btn =
			new widgets::GroupedToolButton(def.groupPosition);
		btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
		btn->setCheckable(true);
		btn->setToolTip(def.toolTip);
		btn->setIcon(QIcon::fromTheme(def.iconName));
		m_logoLocationGroup->addButton(btn, int(def.location));
		logoLayout->addWidget(btn);
	}

	m_logoLocationGroup->button(int(LogoLocation::Default))->setChecked(true);
	connect(
		m_logoLocationGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&TimelapseDialog::updateLogoRect);

	m_formatCombo = new QComboBox;
	settingsForm->addRow(tr("Format:"), m_formatCombo);

	QPair<QString, VideoFormat> formats[] = {
		{QCoreApplication::translate(
			 "dialogs::AnimationExportDialog", "MP4 Video (H.264)"),
		 VideoFormat::Mp4H264},
		{QCoreApplication::translate(
			 "dialogs::AnimationExportDialog", "MP4 Video (AV1)"),
		 VideoFormat::Mp4Av1},
		{QCoreApplication::translate(
			 "dialogs::AnimationExportDialog", "MP4 Video (VP9)"),
		 VideoFormat::Mp4Vp9},
		{QCoreApplication::translate(
			 "dialogs::AnimationExportDialog", "WEBM Video (VP8)"),
		 VideoFormat::WebmVp8},
	};
	bool anyFormatFfmpegSupported = false;
	for(const QPair<QString, VideoFormat> &p : formats) {
		VideoFormat format = p.second;
		bool ffmpegSupported = isVideoFormatSupportedFfmpeg(format);
		if(ffmpegSupported) {
			anyFormatFfmpegSupported = true;
		}

		if(ffmpegSupported || isVideoFormatSupported(format)) {
			m_formatCombo->addItem(p.first, int(format));
		}
	}

	if(anyFormatFfmpegSupported) {
		m_ffmpegNote = new utils::FormNote(
			tr("This format requires FFmpeg, click here to set it up."), false,
			QIcon::fromTheme(QStringLiteral("dialog-warning")), true);
		m_ffmpegNote->hide();
		settingsForm->addRow(nullptr, m_ffmpegNote);
		connect(
			m_ffmpegNote, &utils::FormNote::linkClicked, this,
			&TimelapseDialog::showFfmpegSettings);
	}

	connect(
		m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &TimelapseDialog::updateFfmpeg);

	QHBoxLayout *durationLayout = new QHBoxLayout;
	durationLayout->setContentsMargins(0, 0, 0, 0);
	settingsForm->addRow(tr("Duration:"), durationLayout);

	m_durationEdit = new QTimeEdit;
	m_durationEdit->setMinimumTime(QTime(0, 0, 1));
	m_durationEdit->setMaximumTime(QTime(12, 0, 0));
	m_durationEdit->setDisplayFormat(QStringLiteral("HH:mm:ss"));
	m_durationEdit->setSelectedSection(QDateTimeEdit::SecondSection);
	durationLayout->addWidget(m_durationEdit, 1);

	widgets::GroupedToolButton *durationButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::NotGrouped);
	durationButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	durationButton->setIcon(
		QIcon::fromTheme(QStringLiteral("application-menu")));
	durationButton->setPopupMode(QToolButton::InstantPopup);
	durationLayout->addWidget(durationButton);

	QMenu *durationMenu = new QMenu;
	durationButton->setMenu(durationMenu);
	durationMenu->addSection(tr("Duration presets:"));

	for(int seconds : {30, 45, 60, 90, 120, 150, 180, 300, 600}) {
		QAction *action = durationMenu->addAction(utils::formatTime(seconds));
		connect(
			action, &QAction::triggered, this,
			std::bind(&TimelapseDialog::setDurationSeconds, this, seconds));
	}

	m_widthSpinner = new QSpinBox;
	m_heightSpinner = new QSpinBox;
	m_widthSpinner->setRange(1, 9999);
	m_heightSpinner->setRange(1, 9999);

	QHBoxLayout *resolutionLayout = new QHBoxLayout;
	resolutionLayout->setContentsMargins(0, 0, 0, 0);
	resolutionLayout->setSpacing(8);
	resolutionLayout->addWidget(m_widthSpinner, 1);
	resolutionLayout->addWidget(new QLabel(QStringLiteral("x")));
	resolutionLayout->addWidget(m_heightSpinner, 1);
	resolutionLayout->addWidget(new QLabel(tr("pixels")));
	settingsForm->addRow(tr("Resolution:"), resolutionLayout);

	m_keepAspectCheckBox = new QCheckBox(tr("Keep aspect ratio"));
	m_keepAspectCheckBox->setChecked(true);
	settingsForm->addRow(nullptr, m_keepAspectCheckBox);

	bool haveCrop = !m_crop.isEmpty();
	m_cropCheckBox = new QCheckBox(tr("Crop to selection area"));
	m_cropCheckBox->setEnabled(haveCrop);
	m_cropCheckBox->setChecked(haveCrop);
	settingsForm->addRow(nullptr, m_cropCheckBox);
	if(haveCrop) {
		connect(
			m_cropCheckBox, &QCheckBox::clicked, this,
			&TimelapseDialog::setUseCrop);
	}

	m_dimensionsNote = utils::formNote(
		tr("Video with dimensions larger than 1920 pixels is not widely "
		   "supported. Many devices and platforms won't play them properly, "
		   "degrade their quality or refuse to recognize them altogether."),
		QSizePolicy::Label, QIcon::fromTheme("dialog-warning"));
	m_dimensionsNote->hide();
	settingsForm->addRow(nullptr, m_dimensionsNote);

	connect(
		m_widthSpinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&TimelapseDialog::updateWidth);
	connect(
		m_heightSpinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&TimelapseDialog::updateHeight);
	connect(
		m_keepAspectCheckBox, &QCheckBox::clicked, this,
		&TimelapseDialog::updateAspectRatio);

	if(m_canvasState.hasAnimation()) {
		drawdance::DocumentMetadata documentMetadata =
			m_canvasState.documentMetadata();
		documentMetadata.effectiveFrameRange(
			m_frameRangeFirst, m_frameRangeLast);
		m_animationFramerate = documentMetadata.effectiveFramerate();

		m_animationResultCheckBox =
			new QCheckBox(tr("Play animation as result"));
		m_animationResultCheckBox->setChecked(inFrameView);
		settingsForm->addRow(nullptr, m_animationResultCheckBox);
		connect(
			m_animationResultCheckBox, &QCheckBox::clicked, this,
			&TimelapseDialog::updateAnimation);

		// Only offer the flipbook parameters if they are valid and different
		// from the canvas parameters.
		int frameCount = m_canvasState.frameCount();
		bool flipbookParametersValid =
			flipbookSpeedPercent > 0.0 && flipbookFrameRangeFirst >= 0 &&
			flipbookFrameRangeLast >= 0 &&
			flipbookFrameRangeFirst <= flipbookFrameRangeLast &&
			flipbookFrameRangeLast < frameCount;

		if(flipbookParametersValid) {
			m_flipbookFrameRangeFirst = flipbookFrameRangeFirst;
			m_flipbookFrameRangeLast = flipbookFrameRangeLast;
			m_flipbookFramerate =
				m_animationFramerate * (flipbookSpeedPercent / 100.0);

			bool flipbookParametersDifferent =
				m_flipbookFrameRangeFirst != m_frameRangeFirst ||
				m_flipbookFrameRangeLast != m_frameRangeLast ||
				std::abs(m_flipbookFramerate - m_animationFramerate) >= 0.01;

			if(flipbookParametersDifferent) {
				m_animationFlipbookCheckBox =
					new QCheckBox(tr("Use flipbook range and speed"));
				m_animationFlipbookCheckBox->hide();
				settingsForm->addRow(nullptr, m_animationFlipbookCheckBox);
			}
		}
	}

	if(anyFormatFfmpegSupported) {
		QHBoxLayout *ffmpegButtonLayout = new QHBoxLayout;
		ffmpegButtonLayout->setContentsMargins(0, 0, 0, 0);
		settingsLayout->addLayout(ffmpegButtonLayout);

		m_ffmpegButton = new QPushButton(
			QIcon::fromTheme(QStringLiteral("kdenlive-show-video")), QString());
		utils::setWidgetRetainSizeWhenHidden(m_ffmpegButton, true);
		m_ffmpegButton->setFlat(true);
		m_ffmpegButton->hide();
		ffmpegButtonLayout->addWidget(m_ffmpegButton);
		connect(
			m_ffmpegButton, &QPushButton::clicked, this,
			&TimelapseDialog::showFfmpegSettings);

		ffmpegButtonLayout->addStretch();
	} else {
		utils::addFormSpacer(settingsLayout);
	}

	QHBoxLayout *advancedButtonLayout = new QHBoxLayout;
	advancedButtonLayout->setContentsMargins(0, 0, 0, 0);
	settingsLayout->addLayout(advancedButtonLayout);

	m_advancedButton = new QPushButton(tr("Advanced settings"));
	m_advancedButton->setFlat(true);
	advancedButtonLayout->addWidget(m_advancedButton);
	connect(
		m_advancedButton, &QPushButton::clicked, this,
		&TimelapseDialog::toggleAdvanced);

	advancedButtonLayout->addStretch();

	m_advancedWidget = new QWidget;
	m_advancedWidget->setContentsMargins(0, 0, 0, 0);
	m_advancedWidget->setEnabled(false);
	m_advancedWidget->setVisible(false);
	settingsLayout->addWidget(m_advancedWidget);

	QFormLayout *advancedForm = new QFormLayout(m_advancedWidget);
	advancedForm->setContentsMargins(0, 0, 0, 0);

	if(anyFormatFfmpegSupported) {
		m_ffmpegCheckBox = new QCheckBox(tr("Use FFmpeg if available"));
		advancedForm->addRow(tr("Encoder:"), m_ffmpegCheckBox);
		connect(
			m_ffmpegCheckBox, &QCheckBox::clicked, this,
			&TimelapseDialog::updateFfmpeg);
	}

	QPair<QString, int> interpolationPairs[] = {
		//: Image scaling option that picks an algorithm automatically.
		{tr("Automatic", "interpolation"),
		 int(DP_IMAGE_SCALE_INTERPOLATION_GUESS)},
		//: The name of an image scaling algorithm, lanczos scaling.
		{tr("Lanczos", "interpolation"),
		 int(DP_IMAGE_SCALE_INTERPOLATION_LANCZOS)},
		//: The name of an image scaling algorithm, bicubic scaling.
		{tr("Bicubic", "interpolation"),
		 int(DP_IMAGE_SCALE_INTERPOLATION_BICUBIC)},
		//: The name of an image scaling algorithm, bilinear scaling.
		{tr("Bilinear", "interpolation"),
		 int(DP_IMAGE_SCALE_INTERPOLATION_BILINEAR)},
		//: The name of an image scaling algorithm, for binary artwork.
		{tr("Binary", "interpolation"),
		 int(DP_MSG_TRANSFORM_REGION_MODE_BINARY)},
		//: The name of an image scaling algorithm, nearest-neighbor.
		{tr("Nearest", "interpolation"),
		 int(DP_IMAGE_SCALE_INTERPOLATION_NEAREST)},
	};

	m_interpolationCombo = new QComboBox;
	for(const QPair<QString, int> &p : interpolationPairs) {
		int interpolation = p.second;
		if(DP_image_scale_interpolation_supported(interpolation)) {
			m_interpolationCombo->addItem(p.first, interpolation);
		}
	}
	advancedForm->addRow(tr("Interpolation:"), m_interpolationCombo);

	m_ownCheckBox = new QCheckBox(tr("Only time own drawing"));
	advancedForm->addRow(tr("Timing:"), m_ownCheckBox);

	m_backdropPreview = new color_widgets::ColorPreview;
	m_backdropPreview->setDisplayMode(
		color_widgets::ColorPreview::DisplayMode::AllAlpha);
	m_backdropPreview->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	m_backdropPreview->setCursor(Qt::PointingHandCursor);
	advancedForm->addRow(tr("Backdrop:"), m_backdropPreview);
	connect(
		m_backdropPreview, &color_widgets::ColorPreview::clicked, this,
		&TimelapseDialog::pickBackdropColor);

	m_logoScaleSlider = new KisDoubleSliderSpinBox;
	m_logoScaleSlider->setRange(0.01, 0.5, 3);
	m_logoScaleSlider->setSingleStep(0.01);
	advancedForm->addRow(tr("Logo scale:"), m_logoScaleSlider);
	connect(
		m_logoScaleSlider,
		QOverload<double>::of(&KisDoubleSliderSpinBox::valueChanged), this,
		&TimelapseDialog::updateLogoRect);

	m_logoOffsetSlider = new KisDoubleSliderSpinBox;
	m_logoOffsetSlider->setRange(0.0, 0.5, 3);
	m_logoOffsetSlider->setSingleStep(0.01);
	advancedForm->addRow(tr("Logo padding:"), m_logoOffsetSlider);
	connect(
		m_logoOffsetSlider,
		QOverload<double>::of(&KisDoubleSliderSpinBox::valueChanged), this,
		&TimelapseDialog::updateLogoRect);

	m_logoOpacitySlider = new KisSliderSpinBox;
	m_logoOpacitySlider->setRange(1, 100);
	m_logoOpacitySlider->setSuffix(tr("%"));
	advancedForm->addRow(tr("Logo opacity:"), m_logoOpacitySlider);
	connect(
		m_logoOpacitySlider,
		QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&TimelapseDialog::updateLogoOpacity);

	QString lingerBeforeTitle = tr("Preview result:");
	m_lingerBeforeSlider = new KisDoubleSliderSpinBox;
	m_lingerBeforeSlider->setRange(0.0, 60.0, 2);
	m_lingerBeforeSlider->setSingleStep(0.1);
	m_lingerBeforeSlider->setSuffix(tr(" seconds"));
	kineticScroller->disableKineticScrollingOnWidget(m_lingerBeforeSlider);

	if(m_animationResultCheckBox) {
		m_lingerAnimationBeforeSlider = new KisSliderSpinBox;
		m_lingerAnimationBeforeSlider->setRange(0, 99);
		m_lingerAnimationBeforeSlider->hide();
		bindAnimationLoopSlider(m_lingerAnimationBeforeSlider);
		kineticScroller->disableKineticScrollingOnWidget(
			m_lingerAnimationBeforeSlider);

		QHBoxLayout *lingerBeforeLayout = new QHBoxLayout;
		lingerBeforeLayout->setContentsMargins(0, 0, 0, 0);
		lingerBeforeLayout->setSpacing(0);
		lingerBeforeLayout->addWidget(m_lingerBeforeSlider);
		lingerBeforeLayout->addWidget(m_lingerAnimationBeforeSlider);
		advancedForm->addRow(lingerBeforeTitle, lingerBeforeLayout);
	} else {
		advancedForm->addRow(lingerBeforeTitle, m_lingerBeforeSlider);
	}

	QHBoxLayout *flashLayout = new QHBoxLayout;
	flashLayout->setContentsMargins(0, 0, 0, 0);
	advancedForm->addRow(tr("Flash:"), flashLayout);

	m_flashPreview = new color_widgets::ColorPreview;
	m_flashPreview->setDisplayMode(
		color_widgets::ColorPreview::DisplayMode::AllAlpha);
	m_flashPreview->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	m_flashPreview->setCursor(Qt::PointingHandCursor);
	flashLayout->addWidget(m_flashPreview);
	connect(
		m_flashPreview, &color_widgets::ColorPreview::clicked, this,
		&TimelapseDialog::pickFlashColor);

	m_flashSlider = new KisDoubleSliderSpinBox;
	m_flashSlider->setRange(0.0, 60.0, 2);
	m_flashSlider->setSingleStep(0.1);
	m_flashSlider->setSuffix(tr(" seconds"));
	flashLayout->addWidget(m_flashSlider);
	kineticScroller->disableKineticScrollingOnWidget(m_flashSlider);

	QString lingerAfterTitle = tr("Linger result:");
	m_lingerAfterSlider = new KisDoubleSliderSpinBox;
	m_lingerAfterSlider->setRange(0.0, 60.0, 2);
	m_lingerAfterSlider->setSingleStep(0.1);
	m_lingerAfterSlider->setSuffix(tr(" seconds"));
	kineticScroller->disableKineticScrollingOnWidget(m_lingerAfterSlider);

	if(m_animationResultCheckBox) {
		m_lingerAnimationAfterSlider = new KisSliderSpinBox;
		m_lingerAnimationAfterSlider->setRange(0, 99);
		m_lingerAnimationAfterSlider->hide();
		bindAnimationLoopSlider(m_lingerAnimationAfterSlider);
		kineticScroller->disableKineticScrollingOnWidget(
			m_lingerAnimationAfterSlider);

		QHBoxLayout *lingerAfterLayout = new QHBoxLayout;
		lingerAfterLayout->setContentsMargins(0, 0, 0, 0);
		lingerAfterLayout->setSpacing(0);
		lingerAfterLayout->addWidget(m_lingerAfterSlider);
		lingerAfterLayout->addWidget(m_lingerAnimationAfterSlider);
		advancedForm->addRow(lingerAfterTitle, lingerAfterLayout);
	} else {
		advancedForm->addRow(lingerAfterTitle, m_lingerAfterSlider);
	}

	m_maxDeltaSlider = new KisDoubleSliderSpinBox;
	m_maxDeltaSlider->setRange(0.01, 10.0, 2);
	m_maxDeltaSlider->setSingleStep(0.1);
	m_maxDeltaSlider->setSuffix(tr(" seconds"));
	m_maxDeltaSlider->setExponentRatio(3.0);
	advancedForm->addRow(tr("Interval limit:"), m_maxDeltaSlider);
	kineticScroller->disableKineticScrollingOnWidget(m_maxDeltaSlider);

	m_maxQueueEntriesSlider = new KisSliderSpinBox;
	m_maxQueueEntriesSlider->setRange(1, 100);
	advancedForm->addRow(tr("Queue size:"), m_maxQueueEntriesSlider);
	kineticScroller->disableKineticScrollingOnWidget(m_maxQueueEntriesSlider);

	m_framerateSlider = new KisDoubleSliderSpinBox;
	m_framerateSlider->setRange(1.0, 300.0, 2);
	m_framerateSlider->setExponentRatio(3.0);
	//: Frames per second.
	m_framerateSlider->setSuffix(tr(" FPS"));
	advancedForm->addRow(tr("Framerate:"), m_framerateSlider);
	kineticScroller->disableKineticScrollingOnWidget(m_framerateSlider);

	m_framerateNote = utils::formNote(
		tr("Video with framerates above 30 FPS is not widely "
		   "supported. Many devices and platforms won't play them properly, "
		   "degrade their quality or refuse to recognize them altogether."),
		QSizePolicy::Label, QIcon::fromTheme("dialog-warning"));
	m_framerateNote->setContentsVisible(false);
	advancedForm->addRow(nullptr, m_framerateNote);

	connect(
		m_framerateSlider,
		QOverload<double>::of(&KisDoubleSliderSpinBox::valueChanged), this,
		&TimelapseDialog::updateFramerateNote);

	settingsLayout->addStretch();

	QVBoxLayout *progressLayout = new QVBoxLayout(m_progressPage);
	progressLayout->setContentsMargins(0, 0, 0, 0);
	progressLayout->addStretch();

	m_progressLabel = new QLabel;
	m_progressLabel->setAlignment(Qt::AlignCenter);
	m_progressLabel->setWordWrap(true);
	progressLayout->addWidget(m_progressLabel);

	m_progressBar = new QProgressBar;
	progressLayout->addWidget(m_progressBar);

	progressLayout->addStretch();

	QVBoxLayout *finishLayout = new QVBoxLayout(m_finishPage);
	finishLayout->setContentsMargins(0, 0, 0, 0);
	finishLayout->addStretch();

	m_finishLabel = new QLabel;
	m_finishLabel->setAlignment(Qt::AlignCenter);
	m_finishLabel->setWordWrap(true);
	finishLayout->addWidget(m_finishLabel);

	finishLayout->addStretch();

	m_buttons = new QDialogButtonBox();
	layout->addWidget(m_buttons);

	showSettingsPage();
	setDefaultResolutions();
	updateCurrentResolution();
	updateAnimation();
	loadSettings();
	updateFfmpeg();
}

void TimelapseDialog::setInputPath(const QString &inputPath)
{
	m_inputPath = inputPath;
	updateExportButton();
}

void TimelapseDialog::accept()
{
	if(!m_saver) {
		int format = m_formatCombo->currentData().toInt();

		bool useFfmpeg;
		if(isVideoFormatSupported(VideoFormat(format))) {
			useFfmpeg = m_ffmpegCheckBox && m_ffmpegCheckBox->isChecked() &&
						!m_ffmpegPath.isEmpty();
		} else {
			useFfmpeg = true;
		}

		if(useFfmpeg & m_ffmpegPath.isEmpty()) {
			QMessageBox *box = utils::showQuestion(
				this, tr("FFmpeg"),
				tr("The selected format requires FFmpeg. Do you want to set it "
				   "up now?"));
			connect(
				box, &QMessageBox::accepted, this,
				&TimelapseDialog::showFfmpegSettings);
			return;
		}

		saveSettings();

		QString outputPath = choosePath(format);
		if(!outputPath.isEmpty()) {
			bool lingerAnimation = m_animationResultCheckBox &&
								   m_animationResultCheckBox->isChecked();
			double flashSeconds = m_flashSlider->value();
			double playbackSeconds = getDurationSeconds() - flashSeconds;

			double lingerBeforeSeconds, lingerAfterSeconds;
			int lingerBeforeLoops, lingerAfterLoops;
			int frameRangeFirst, frameRangeLast;
			double animationFramerate;
			if(lingerAnimation) {
				lingerBeforeSeconds = 0.0;
				lingerAfterSeconds = 0.0;
				lingerBeforeLoops = m_lingerAnimationBeforeSlider->value();
				lingerAfterLoops = m_lingerAnimationAfterSlider->value();

				if(m_animationFlipbookCheckBox &&
				   m_animationFlipbookCheckBox->isChecked()) {
					frameRangeFirst = m_flipbookFrameRangeFirst;
					frameRangeLast = m_flipbookFrameRangeLast;
					animationFramerate = m_flipbookFramerate;
				} else {
					frameRangeFirst = m_frameRangeFirst;
					frameRangeLast = m_frameRangeLast;
					animationFramerate = m_animationFramerate;
				}

				int frames = frameRangeLast - frameRangeFirst + 1;
				playbackSeconds -=
					qreal(lingerBeforeLoops * frames) / animationFramerate;
				playbackSeconds -=
					qreal(lingerAfterLoops * frames) / animationFramerate;
			} else {
				lingerBeforeSeconds = m_lingerBeforeSlider->value();
				lingerAfterSeconds = m_lingerAfterSlider->value();
				lingerBeforeLoops = 0;
				lingerAfterLoops = 0;
				frameRangeFirst = -1;
				frameRangeLast = -2;
				animationFramerate = 0.0;
				playbackSeconds -= lingerBeforeSeconds;
				playbackSeconds -= lingerAfterSeconds;
			}

			if(playbackSeconds < 1.0) {
				playbackSeconds = 1.0;
			}

			const config::Config *cfg = dpAppConfig();
			m_saver = new TimelapseSaverRunnable(
				m_canvasState, &m_vmf, useFfmpeg ? m_ffmpegPath : QString(),
				outputPath, m_inputPath, format, m_widthSpinner->value(),
				m_heightSpinner->value(),
				m_interpolationCombo->currentData().toInt(),
				m_cropCheckBox->isChecked() ? m_crop : QRect(),
				m_backdropPreview->color(), cfg->getCheckerColor1(),
				cfg->getCheckerColor2(), m_flashPreview->color(), getLogoRect(),
				double(m_logoOpacitySlider->value()) / 100.0, getLogoImage(),
				m_framerateSlider->value(), lingerBeforeSeconds,
				playbackSeconds, flashSeconds, lingerAfterSeconds,
				m_maxDeltaSlider->value(), m_maxQueueEntriesSlider->value(),
				m_ownCheckBox->isChecked(), lingerBeforeLoops, lingerAfterLoops,
				frameRangeFirst, frameRangeLast, animationFramerate);
			m_saver->setAutoDelete(false);

			connect(
				m_saver, &TimelapseSaverRunnable::stepChanged, this,
				&TimelapseDialog::updateProgressLabel, Qt::QueuedConnection);
			connect(
				m_saver, &TimelapseSaverRunnable::progress, this,
				&TimelapseDialog::setProgress, Qt::QueuedConnection);
			connect(
				m_saver, &TimelapseSaverRunnable::frameProgress,
				m_timelapsePreview,
				&widgets::TimelapsePreview::setRenderedFrame,
				Qt::QueuedConnection);
			connect(
				m_saver, &TimelapseSaverRunnable::saveComplete, this,
				&TimelapseDialog::handleSaveComplete, Qt::QueuedConnection);
			connect(
				m_saver, &TimelapseSaverRunnable::saveCancelled, this,
				&TimelapseDialog::handleSaveCancelled, Qt::QueuedConnection);
			connect(
				m_saver, &TimelapseSaverRunnable::saveFailed, this,
				&TimelapseDialog::handleSaveFailed, Qt::QueuedConnection);

			m_cancelling = false;
			m_progressLabel->setText(tr("Starting export…"));
			m_progressBar->reset();
			m_progressBar->setRange(0, 0);
			m_timelapsePreview->setAcceptRender(true);
			showProgressPage();

			QThreadPool::globalInstance()->start(m_saver);
		}
	}
}

void TimelapseDialog::reject()
{
	if(m_saver) {
		m_cancelling = true;
		m_progressLabel->setText(tr("Cancelling…"));
		m_saver->cancelExport();
	} else {
		saveSettings();
		QDialog::reject();
	}
}

void TimelapseDialog::setDefaultResolutions()
{
	m_fullResolution = calculateResolution(m_canvasState.size());
	if(m_crop.isEmpty()) {
		m_cropResolution = m_fullResolution;
	} else {
		m_cropResolution = calculateResolution(m_crop.size());
	}
}

QSize TimelapseDialog::calculateResolution(const QSize &inputSize)
{
	QSize size = drawdance::thumbnailDimensions(
		inputSize, QSize(
					   qMin(inputSize.width(), DEFAULT_MAX_WIDTH),
					   qMin(inputSize.height(), DEFAULT_MAX_HEIGHT)));
	if(size.isEmpty()) {
		return QSize(DEFAULT_FALLBACK_WIDTH, DEFAULT_FALLBACK_HEIGHT);
	} else {
		return size;
	}
}

void TimelapseDialog::updateCurrentResolution()
{
	QSize currentSize;
	if(m_cropCheckBox->isChecked()) {
		currentSize = m_cropResolution;
	} else {
		currentSize = m_fullResolution;
	}

	QSignalBlocker widthBlocker(m_widthSpinner);
	QSignalBlocker heightBlocker(m_heightSpinner);
	m_widthSpinner->setValue(currentSize.width());
	m_heightSpinner->setValue(currentSize.height());
	updateAspectRatio(m_keepAspectCheckBox->isChecked());
	updatePreviewCrop();
	updatePreviewSize();
	updateDimensionsNote();
}

void TimelapseDialog::updateAnimation()
{
	if(m_animationResultCheckBox) {
		utils::ScopedUpdateDisabler disabler(this);
		if(m_animationResultCheckBox->isChecked()) {
			m_lingerBeforeSlider->hide();
			m_lingerAfterSlider->hide();
			if(m_animationFlipbookCheckBox) {
				m_animationFlipbookCheckBox->show();
			}
			m_lingerAnimationBeforeSlider->show();
			m_lingerAnimationAfterSlider->show();
		} else {
			if(m_animationFlipbookCheckBox) {
				m_animationFlipbookCheckBox->hide();
			}
			m_lingerAnimationBeforeSlider->hide();
			m_lingerAnimationAfterSlider->hide();
			m_lingerAfterSlider->show();
			m_lingerBeforeSlider->show();
		}
	}
}

void TimelapseDialog::setUseCrop(bool checked)
{
	QSize sizeToSave(m_widthSpinner->value(), m_heightSpinner->value());
	QSize sizeToSet;
	if(checked) {
		m_fullResolution = sizeToSave;
		sizeToSet = m_cropResolution;
	} else {
		m_cropResolution = sizeToSave;
		sizeToSet = m_fullResolution;
	}

	QSignalBlocker widthBlocker(m_widthSpinner);
	QSignalBlocker heightBlocker(m_heightSpinner);
	m_widthSpinner->setValue(sizeToSet.width());
	m_heightSpinner->setValue(sizeToSet.height());
	updateAspectRatio(m_keepAspectCheckBox->isChecked());
	updatePreviewCrop();
	updateLogoRect();
	updatePreviewSize();
	updateDimensionsNote();
}

void TimelapseDialog::resetToDefaultSettings()
{
	m_keepAspectCheckBox->setChecked(true);
	m_cropCheckBox->setChecked(!m_crop.isEmpty());
	setDefaultResolutions();
	updateCurrentResolution();
	resetDefaultExportFormat();
	setDurationSeconds(config::Config::defaultTimelapseDurationSeconds());
	if(!checkLogoLocation(config::Config::defaultTimelapseLogoLocation())) {
		checkLogoLocation(int(LogoLocation::Default));
	}
	if(m_ffmpegCheckBox) {
		m_ffmpegCheckBox->setChecked(
			config::Config::defaultTimelapsePreferFfmpeg());
	}
	if(!selectInterpolation(config::Config::defaultTimelapseInterpolation())) {
		m_interpolationCombo->setCurrentIndex(0);
	}
	m_ownCheckBox->setChecked(config::Config::defaultTimelapseTimeOwnOnly());
	m_backdropPreview->setColor(
		config::Config::defaultTimelapseBackdropColor());
	QSignalBlocker logoScaleBlocker(m_logoScaleSlider);
	m_logoScaleSlider->setValue(config::Config::defaultTimelapseLogoScale());
	QSignalBlocker logoOffsetBlocker(m_logoOffsetSlider);
	m_logoOffsetSlider->setValue(config::Config::defaultTimelapseLogoOffset());
	QSignalBlocker logoOpacityBlocker(m_logoOpacitySlider);
	m_logoOpacitySlider->setValue(
		config::Config::defaultTimelapseLogoOpacity());
	m_lingerBeforeSlider->setValue(
		config::Config::defaultTimelapseLingerBeforeSeconds());
	m_flashPreview->setColor(config::Config::defaultTimelapseFlashColor());
	m_flashSlider->setValue(config::Config::defaultTimelapseFlashSeconds());
	m_lingerAfterSlider->setValue(
		config::Config::defaultTimelapseLingerAfterSeconds());
	m_maxDeltaSlider->setValue(
		config::Config::defaultTimelapseMaxDeltaSeconds());
	m_maxQueueEntriesSlider->setValue(
		config::Config::defaultTimelapseMaxQueueEntries());
	QSignalBlocker framerateBlocker(m_framerateSlider);
	m_framerateSlider->setValue(config::Config::defaultTimelapseFramerate());

	if(m_animationResultCheckBox) {
		m_animationResultCheckBox->setChecked(m_inFrameView);
		if(m_animationFlipbookCheckBox) {
			m_animationFlipbookCheckBox->setChecked(false);
		}
		m_lingerAnimationBeforeSlider->setValue(
			config::Config::defaultTimelapseLingerBeforeLoops());
		m_lingerAnimationAfterSlider->setValue(
			config::Config::defaultTimelapseLingerAfterLoops());
		updateAnimation();
	}

	updateLogoRect();
	updateLogoOpacity(m_logoOpacitySlider->value());
	updateFramerateNote();
	updateFfmpeg();
}

void TimelapseDialog::resetDefaultExportFormat()
{
	int formats[] = {
		int(VideoFormat::Mp4H264),
		int(VideoFormat::Mp4Av1),
		int(VideoFormat::Mp4Vp9),
		int(VideoFormat::WebmVp8),
	};
	// Pick a format that doesn't require ffmpeg if possible.
	for(int format : formats) {
		if(isVideoFormatSupported(VideoFormat(format)) &&
		   selectExportFormat(format)) {
			return;
		}
	}
	// Otherwise fall back to whatever we are able to use.
	for(int format : formats) {
		if(selectExportFormat(format)) {
			return;
		}
	}
}

void TimelapseDialog::loadSettings()
{
	config::Config *cfg = dpAppConfig();
	m_ffmpegPath = cfg->getFfmpegPath();
	if(!selectExportFormat(cfg->getTimelapseExportFormat())) {
		resetDefaultExportFormat();
	}
	setDurationSeconds(cfg->getTimelapseDurationSeconds());
	if(!checkLogoLocation(cfg->getTimelapseLogoLocation())) {
		checkLogoLocation(int(LogoLocation::Default));
	}
	updateAdvanced(cfg->getTimelapseShowAdvanced());
	if(m_ffmpegCheckBox) {
		m_ffmpegCheckBox->setChecked(cfg->getTimelapsePreferFfmpeg());
	}
	if(!selectInterpolation(cfg->getTimelapseLogoLocation())) {
		m_interpolationCombo->setCurrentIndex(0);
	}
	m_ownCheckBox->setChecked(cfg->getTimelapseTimeOwnOnly());
	m_backdropPreview->setColor(cfg->getTimelapseBackdropColor());
	QSignalBlocker logoScaleBlocker(m_logoScaleSlider);
	m_logoScaleSlider->setValue(cfg->getTimelapseLogoScale());
	QSignalBlocker logoOffsetBlocker(m_logoOffsetSlider);
	m_logoOffsetSlider->setValue(cfg->getTimelapseLogoOffset());
	QSignalBlocker logoOpacityBlocker(m_logoOpacitySlider);
	m_logoOpacitySlider->setValue(cfg->getTimelapseLogoOpacity());
	m_lingerBeforeSlider->setValue(cfg->getTimelapseLingerBeforeSeconds());
	m_flashPreview->setColor(cfg->getTimelapseFlashColor());
	m_flashSlider->setValue(cfg->getTimelapseFlashSeconds());
	m_lingerAfterSlider->setValue(cfg->getTimelapseLingerAfterSeconds());
	m_maxDeltaSlider->setValue(cfg->getTimelapseMaxDeltaSeconds());
	m_maxQueueEntriesSlider->setValue(cfg->getTimelapseMaxQueueEntries());
	QSignalBlocker framerateBlocker(m_framerateSlider);
	m_framerateSlider->setValue(cfg->getTimelapseFramerate());

	if(m_animationResultCheckBox) {
		m_animationResultCheckBox->setChecked(m_inFrameView);
		m_lingerAnimationBeforeSlider->setValue(
			cfg->getTimelapseLingerBeforeLoops());
		m_lingerAnimationAfterSlider->setValue(
			cfg->getTimelapseLingerAfterLoops());
		updateAnimation();
	}

	updateLogoRect();
	updateLogoOpacity(m_logoOpacitySlider->value());
	updateFramerateNote();
}

void TimelapseDialog::saveSettings()
{
	config::Config *cfg = dpAppConfig();
	cfg->setTimelapseExportFormat(m_formatCombo->currentData().toInt());
	cfg->setTimelapseDurationSeconds(getDurationSeconds());
	cfg->setTimelapseLogoLocation(m_logoLocationGroup->checkedId());
	cfg->setTimelapseShowAdvanced(m_advancedWidget->isEnabled());
	if(m_ffmpegCheckBox) {
		cfg->setTimelapsePreferFfmpeg(m_ffmpegCheckBox->isChecked());
	}
	cfg->setTimelapseInterpolation(m_interpolationCombo->currentData().toInt());
	cfg->setTimelapseTimeOwnOnly(m_ownCheckBox->isChecked());
	cfg->setTimelapseBackdropColor(m_backdropPreview->color());
	cfg->setTimelapseLogoScale(m_logoScaleSlider->value());
	cfg->setTimelapseLogoOffset(m_logoOffsetSlider->value());
	cfg->setTimelapseLogoOpacity(m_logoOpacitySlider->value());
	cfg->setTimelapseLingerBeforeSeconds(m_lingerBeforeSlider->value());
	cfg->setTimelapseFlashColor(m_flashPreview->color());
	cfg->setTimelapseFlashSeconds(m_flashSlider->value());
	cfg->setTimelapseLingerAfterSeconds(m_lingerAfterSlider->value());
	cfg->setTimelapseMaxDeltaSeconds(m_maxDeltaSlider->value());
	cfg->setTimelapseMaxQueueEntries(m_maxQueueEntriesSlider->value());
	cfg->setTimelapseFramerate(m_framerateSlider->value());
	if(m_animationResultCheckBox) {
		cfg->setTimelapseLingerBeforeLoops(
			m_lingerAnimationBeforeSlider->value());
		cfg->setTimelapseLingerAfterLoops(
			m_lingerAnimationAfterSlider->value());
	}
}

bool TimelapseDialog::selectExportFormat(int format)
{
	int count = m_formatCombo->count();
	for(int i = 0; i < count; ++i) {
		if(m_formatCombo->itemData(i).toInt() == format) {
			m_formatCombo->setCurrentIndex(i);
			return true;
		}
	}
	return false;
}

bool TimelapseDialog::checkLogoLocation(int location)
{
	QAbstractButton *btn = m_logoLocationGroup->button(location);
	if(btn) {
		btn->setChecked(true);
		return true;
	} else {
		return false;
	}
}

bool TimelapseDialog::selectInterpolation(int interpolation)
{
	int count = m_interpolationCombo->count();
	for(int i = 0; i < count; ++i) {
		if(m_interpolationCombo->itemData(i).toInt() == interpolation) {
			m_interpolationCombo->setCurrentIndex(i);
			return true;
		}
	}
	return false;
}

int TimelapseDialog::getDurationSeconds() const
{
	return QTime(0, 0).secsTo(m_durationEdit->time());
}

void TimelapseDialog::setDurationSeconds(int seconds)
{
	m_durationEdit->setTime(QTime(0, 0).addSecs(seconds));
}

void TimelapseDialog::updateFfmpeg()
{
	if(m_ffmpegNote || m_ffmpegButton) {
		int format = m_formatCombo->currentData().toInt();
		bool needsFfmpeg = !isVideoFormatSupported(VideoFormat(format));
		if(m_ffmpegNote) {
			m_ffmpegNote->setVisible(needsFfmpeg && m_ffmpegPath.isEmpty());
		}
		if(m_ffmpegButton) {
			if(m_ffmpegPath.isEmpty()) {
				m_ffmpegButton->setText(tr("Set up FFmpeg"));
			} else {
				m_ffmpegButton->setText(tr("FFmpeg settings"));
			}
			m_ffmpegButton->setVisible(
				needsFfmpeg ||
				(m_ffmpegCheckBox && m_ffmpegCheckBox->isChecked()));
		}
	}
}

void TimelapseDialog::updateWidth(int value)
{
	if(m_aspectRatio > 0.0) {
		QSignalBlocker blocker(m_heightSpinner);
		m_heightSpinner->setValue(qRound(qreal(value) / m_aspectRatio));
	}
	updatePreviewSize();
	updateLogoRect();
	updateDimensionsNote();
}

void TimelapseDialog::updateHeight(int value)
{
	if(m_aspectRatio > 0.0) {
		QSignalBlocker blocker(m_widthSpinner);
		m_widthSpinner->setValue(qRound(qreal(value) * m_aspectRatio));
	}
	updatePreviewSize();
	updateLogoRect();
	updateDimensionsNote();
}

void TimelapseDialog::updateAspectRatio(bool checked)
{
	if(checked) {
		int width = m_widthSpinner->value();
		int height = m_heightSpinner->value();
		if(width > 0 && height > 0) {
			m_aspectRatio = qreal(width) / qreal(height);
		} else {
			m_aspectRatio = 0.0;
		}
	} else {
		m_aspectRatio = 0.0;
	}
}

void TimelapseDialog::updateLogoRect()
{
	m_timelapsePreview->setLogoRect(getLogoRect());
}

void TimelapseDialog::updateLogoOpacity(int opacity)
{
	m_timelapsePreview->setLogoOpacity(double(opacity) / 100.0);
}

void TimelapseDialog::updatePreviewSize()
{
	m_timelapsePreview->setOutputSize(getOutputSize());
}

void TimelapseDialog::updatePreviewCrop()
{
	m_timelapsePreview->setCropRect(
		m_cropCheckBox->isChecked() ? m_crop : QRect());
}

void TimelapseDialog::updateDimensionsNote()
{
	m_dimensionsNote->setVisible(
		m_widthSpinner->value() > 1920 || m_heightSpinner->value() > 1920);
}

void TimelapseDialog::toggleAdvanced()
{
	updateAdvanced(!m_advancedWidget->isEnabled());
}

void TimelapseDialog::updateAdvanced(bool enabled)
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

void TimelapseDialog::updateFramerateNote()
{
	m_framerateNote->setContentsVisible(m_framerateSlider->value() > 30);
}

void TimelapseDialog::bindAnimationLoopSlider(KisSliderSpinBox *slider)
{
	connect(
		slider, QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		[this, slider](int value) {
			updateAnimationLoopText(slider, value);
		});
	updateAnimationLoopText(slider, slider->value());
}

void TimelapseDialog::updateAnimationLoopText(
	KisSliderSpinBox *slider, int value)
{
	utils::encapsulateSpinBoxPrefixSuffix(
		slider, tr("%1 loop(s)", nullptr, value));
}

QSize TimelapseDialog::getOutputSize() const
{
	return QSize(m_widthSpinner->value(), m_heightSpinner->value());
}

QRect TimelapseDialog::getLogoRect()
{
	int location = m_logoLocationGroup->checkedId();
	if(location != int(LogoLocation::None)) {
		const QImage &img = getLogoImage();
		QSize outputSize = getOutputSize();
		QSize logoSize = img.size().scaled(
			(QSizeF(outputSize) * m_logoScaleSlider->value()).toSize(),
			Qt::KeepAspectRatio);

		if(!logoSize.isEmpty()) {
			QRect outputRect(QPoint(0, 0), outputSize);
			QRect logoRect(QPoint(0, 0), logoSize);
			qreal logoOffsetRatio = m_logoOffsetSlider->value();
			int offset = qMin(
				qRound(outputSize.width() * logoOffsetRatio),
				qRound(outputSize.height() * logoOffsetRatio));

			switch(location) {
			case int(LogoLocation::TopLeft):
				logoRect.moveTopLeft(QPoint(offset, offset));
				break;
			case int(LogoLocation::TopRight):
				logoRect.moveTopRight(
					QPoint(outputRect.right() - offset, offset));
				break;
			case int(LogoLocation::BottomLeft):
				logoRect.moveBottomLeft(
					QPoint(offset, outputRect.bottom() - offset));
				break;
			default:
				logoRect.moveBottomRight(QPoint(
					outputRect.right() - offset, outputRect.bottom() - offset));
				break;
			}

			return logoRect;
		}
	}
	return QRect();
}

const QImage &TimelapseDialog::getLogoImage()
{
	if(m_logoImage.isNull() || m_logoImage.size().isEmpty()) {
		QString logoName = QStringLiteral("logo1.png");
		QString logoPath = utils::paths::locateDataFile(logoName);
		if(logoPath.isEmpty()) {
			qWarning("Logo data file '%s' not found", qUtf8Printable(logoName));
		} else if(!m_logoImage.load(logoPath)) {
			qWarning("Failed to load logo from '%s'", qUtf8Printable(logoPath));
		}

		if(m_logoImage.isNull() || m_logoImage.size().isEmpty()) {
			m_logoImage = QImage(392, 110, QImage::Format_ARGB32_Premultiplied);
			m_logoImage.fill(Qt::red);
		}

		m_timelapsePreview->setLogoImage(m_logoImage);
	}
	return m_logoImage;
}

void TimelapseDialog::showFfmpegSettings()
{
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
			&TimelapseDialog::setFfmpegPath);
		dlg->show();
	}
}

void TimelapseDialog::setFfmpegPath(const QString &ffmpegPath)
{
	if(ffmpegPath != m_ffmpegPath) {
		m_ffmpegPath = ffmpegPath;
		updateFfmpeg();
	}
}

void TimelapseDialog::pickBackdropColor()
{
	pickColor(
		QStringLiteral("backdropcolordialog"), tr("Choose Backdrop Color"),
		m_backdropPreview, config::Config::defaultTimelapseBackdropColor());
}

void TimelapseDialog::pickFlashColor()
{
	pickColor(
		QStringLiteral("flashcolordialog"), tr("Choose Flash Color"),
		m_flashPreview, config::Config::defaultTimelapseFlashColor());
}

void TimelapseDialog::pickColor(
	const QString &objectName, const QString &title,
	color_widgets::ColorPreview *preview, const QColor &resetColor)
{
	color_widgets::ColorDialog *dlg = findChild<color_widgets::ColorDialog *>(
		objectName, Qt::FindDirectChildrenOnly);
	if(dlg) {
		dlg->activateWindow();
		dlg->raise();
	} else {
		dlg = dialogs::newDeleteOnCloseColorDialog(preview->color(), this);
		dlg->setObjectName(objectName);
		dlg->setWindowTitle(title);
		utils::makeModal(dlg);
		dialogs::setColorDialogResetColor(dlg, resetColor);
		connect(
			dlg, &color_widgets::ColorDialog::colorSelected, preview,
			&color_widgets::ColorPreview::setColor);
		dlg->show();
	}
}

void TimelapseDialog::confirmReset()
{
	QMessageBox *box = utils::showQuestion(
		this, tr("Reset"),
		tr("Are you sure you want to reset all timelapse settings to their "
		   "default values?"));
	connect(
		box, &QMessageBox::accepted, this,
		&TimelapseDialog::resetToDefaultSettings);
}

QString TimelapseDialog::choosePath(int format)
{
	switch(VideoFormat(format)) {
	case VideoFormat::Mp4Vp9:
	case VideoFormat::Mp4H264:
	case VideoFormat::Mp4Av1:
		return FileWrangler(this).getSaveAnimationMp4Path();
	case VideoFormat::WebmVp8:
		return FileWrangler(this).getSaveAnimationWebmPath();
	case VideoFormat::Frames:
	case VideoFormat::Zip:
	case VideoFormat::Webp:
	case VideoFormat::Gif:
		break;
	}
	qWarning("choosePath: unhandled format %d", format);
	return QString();
}

void TimelapseDialog::showSettingsPage()
{
	utils::ScopedUpdateDisabler disabler(this);
	m_buttons->clear();
	m_progressPage->hide();
	m_finishPage->hide();
	m_settingsPage->show();

	QPushButton *okButton = m_buttons->addButton(QDialogButtonBox::Ok);
	connect(okButton, &QPushButton::clicked, this, &TimelapseDialog::accept);

	QPushButton *cancelButton = m_buttons->addButton(QDialogButtonBox::Cancel);
	connect(
		cancelButton, &QPushButton::clicked, this, &TimelapseDialog::reject);

	QPushButton *resetButton = m_buttons->addButton(QDialogButtonBox::Reset);
	connect(
		resetButton, &QPushButton::clicked, this,
		&TimelapseDialog::confirmReset);

	updateExportButton();
}

void TimelapseDialog::showProgressPage()
{
	utils::ScopedUpdateDisabler disabler(this);
	m_buttons->clear();
	m_settingsPage->hide();
	m_finishPage->hide();
	m_progressPage->show();

	QPushButton *cancelButton = m_buttons->addButton(QDialogButtonBox::Cancel);
	connect(
		cancelButton, &QPushButton::clicked, this, &TimelapseDialog::reject);
}

void TimelapseDialog::showFinishPage()
{
	utils::ScopedUpdateDisabler disabler(this);
	m_buttons->clear();
	m_settingsPage->hide();
	m_progressPage->hide();
	m_finishPage->show();

	QPushButton *closeButton = m_buttons->addButton(QDialogButtonBox::Close);
	connect(closeButton, &QPushButton::clicked, this, &TimelapseDialog::reject);

	QPushButton *retryButton = m_buttons->addButton(QDialogButtonBox::Retry);
	retryButton->setText(tr("Back"));
	connect(
		retryButton, &QPushButton::clicked, this,
		&TimelapseDialog::showSettingsPage);
}

void TimelapseDialog::updateExportButton()
{
	QPushButton *okButton = m_buttons->button(QDialogButtonBox::Ok);
	if(okButton) {
		bool saveInProgress = m_inputPath.isEmpty();
		okButton->setText(saveInProgress ? tr("Saving…") : tr("Export"));
		okButton->setDisabled(saveInProgress);
	}
}

void TimelapseDialog::updateProgressLabel(const QString &message)
{
	if(!m_cancelling) {
		m_progressLabel->setText(message);
	}
}

void TimelapseDialog::setProgress(int percent)
{
	if(m_progressBar->maximum() == 0) {
		m_progressBar->setRange(0, 100);
		m_progressBar->resetFormat();
	}
	m_progressBar->setValue(percent);
}

void TimelapseDialog::handleSaveComplete(qint64 msecs)
{
	if(m_saver) {
		m_timelapsePreview->setAcceptRender(false);
		m_saver->deleteLater();
		m_saver = nullptr;
		m_finishLabel->setText(
			//: %1 is a time, like "1 minute, 20 seconds".
			tr("Timelapse exported in %1.")
				.arg(utils::formatTime(qRound(qreal(msecs) / 1000.0))));
		showFinishPage();
	}
}

void TimelapseDialog::handleSaveCancelled()
{
	if(m_saver) {
		m_timelapsePreview->setAcceptRender(false);
		m_saver->deleteLater();
		m_saver = nullptr;
		showSettingsPage();
	}
}

void TimelapseDialog::handleSaveFailed(const QString &errorMessage)
{

	if(m_saver) {
		m_timelapsePreview->setAcceptRender(false);
		m_saver->deleteLater();
		m_saver = nullptr;
		utils::showWarning(
			this, tr("Export Error"), tr("Failed to export timelapse."),
			errorMessage);
		showSettingsPage();
	}
}

}
