// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/projectplaybackdialog.h"
#include "desktop/dialogs/projectdialog.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/noscroll.h"
#include "libclient/import/recordingconverter.h"
#include "libclient/project/projectwrangler.h"
#include "libclient/utils/pathinfo.h"
#include "libclient/utils/qtguicompat.h"
#include "libclient/utils/strings.h"
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QSlider>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QTemporaryFile>
#include <QThreadPool>
#include <QTimer>
#include <QVBoxLayout>

namespace {

class PlaybackSlider : public widgets::NoScrollSlider {
public:
	explicit PlaybackSlider(QWidget *parent = nullptr)
		: widgets::NoScrollSlider(parent)
	{
		setOrientation(Qt::Horizontal);
		setRange(0, 10000);
		setPageStep(1000);
		setSingleStep(100);
	}

protected:
	void mousePressEvent(QMouseEvent *event) override
	{
		if(event->button() == Qt::LeftButton) {
			QStyleOptionSlider opt;
			initStyleOption(&opt);
			int mouseX = compat::mousePos(*event).x();
			setValue(
				QStyle::sliderValueFromPosition(
					minimum(), maximum(), mouseX, width(), opt.upsideDown));
			event->accept();
		}
		widgets::NoScrollSlider::mousePressEvent(event);
		setRepeatAction(QAbstractSlider::SliderNoAction);
	}
};

}

namespace dialogs {

ProjectPlaybackDialog::ProjectPlaybackDialog(QWidget *parent)
	: QDialog(parent)
	, m_playTip(tr("Play"))
	, m_pauseTip(tr("Pause"))
	, m_playIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")))
	, m_pauseIcon(QIcon::fromTheme(QStringLiteral("media-playback-pause")))
{
	setWindowFlags(Qt::Tool);
	resize(420, 250);

	QVBoxLayout *dlgLayout = new QVBoxLayout(this);

	m_stack = new QStackedWidget;
	m_stack->setContentsMargins(0, 0, 0, 0);
	dlgLayout->addWidget(m_stack, 1);

	m_messagePage = new QWidget;
	m_messagePage->setContentsMargins(0, 0, 0, 0);
	m_stack->addWidget(m_messagePage);

	QVBoxLayout *messageLayout = new QVBoxLayout(m_messagePage);
	messageLayout->setContentsMargins(0, 0, 0, 0);

	messageLayout->addStretch();

	m_messageLabel = new QLabel(tr("Loading…"));
	m_messageLabel->setAlignment(Qt::AlignCenter);
	m_messageLabel->setWordWrap(true);
	messageLayout->addWidget(m_messageLabel);

	m_messageBar = new QProgressBar;
	m_messageBar->setRange(0, 0);
	messageLayout->addWidget(m_messageBar);

	messageLayout->addStretch();

	m_playbackPage = new QWidget;
	m_playbackPage->setContentsMargins(0, 0, 0, 0);
	m_stack->addWidget(m_playbackPage);

	QVBoxLayout *playbackLayout = new QVBoxLayout(m_playbackPage);
	playbackLayout->setContentsMargins(0, 0, 0, 0);

	QHBoxLayout *buttonsLayout = new QHBoxLayout;
	buttonsLayout->setSpacing(0);
	playbackLayout->addLayout(buttonsLayout);

	buttonsLayout->addStretch(1);

	m_rewindButton = makePlaybackButton(
		int(widgets::GroupedToolButton::GroupLeft), QStringLiteral("go-first"),
		tr("Rewind"));
	buttonsLayout->addWidget(m_rewindButton, 1);
	connect(
		m_rewindButton, &widgets::GroupedToolButton::clicked, this,
		&ProjectPlaybackDialog::onRewindClicked);

	m_backwardButton = makePlaybackButton(
		int(widgets::GroupedToolButton::GroupCenter),
		QStringLiteral("go-previous-skip"), tr("Step backward"));
	buttonsLayout->addWidget(m_backwardButton, 1);
	connect(
		m_backwardButton, &widgets::GroupedToolButton::clicked, this,
		&ProjectPlaybackDialog::onBackwardClicked);

	m_playPauseButton = makePlaybackButton(
		int(widgets::GroupedToolButton::GroupCenter), QString(), QString());
	buttonsLayout->addWidget(m_playPauseButton, 2);
	connect(
		m_playPauseButton, &widgets::GroupedToolButton::clicked, this,
		&ProjectPlaybackDialog::onPlayPauseClicked);

	m_stepButton = makePlaybackButton(
		int(widgets::GroupedToolButton::GroupCenter), QStringLiteral("go-next"),
		tr("Step next drawing command"));
	buttonsLayout->addWidget(m_stepButton, 1);
	connect(
		m_stepButton, &widgets::GroupedToolButton::clicked, this,
		&ProjectPlaybackDialog::onStepClicked);

	m_skipButton = makePlaybackButton(
		int(widgets::GroupedToolButton::GroupRight),
		QStringLiteral("go-next-skip"), tr("Step next stroke"));
	buttonsLayout->addWidget(m_skipButton, 1);
	connect(
		m_skipButton, &widgets::GroupedToolButton::clicked, this,
		&ProjectPlaybackDialog::onSkipClicked);

	buttonsLayout->addStretch(1);

	m_playbackSpeedSlider = new widgets::NoScrollKisSliderSpinBox;
	m_playbackSpeedSlider->setRange(1, 10000);
	m_playbackSpeedSlider->setExponentRatio(3.0);
	m_playbackSpeedSlider->setValue(100);
	m_playbackSpeedSlider->setPrefix(tr("Playback speed: "));
	m_playbackSpeedSlider->setSuffix(strings::percent());
	playbackLayout->addWidget(m_playbackSpeedSlider);

	m_progressSlider = new PlaybackSlider;
	playbackLayout->addWidget(m_progressSlider);
	connect(
		m_progressSlider, QOverload<int>::of(&QSlider::valueChanged), this,
		&ProjectPlaybackDialog::onProgressValueChanged);
	connect(
		m_progressSlider, &QSlider::sliderReleased, this,
		&ProjectPlaybackDialog::onProgressSliderReleased);

	m_progressLabel = new QLabel;
	m_progressLabel->setWordWrap(true);
	m_progressLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
	playbackLayout->addWidget(m_progressLabel, 1);

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
	dlgLayout->addWidget(buttons);
	connect(
		buttons, &QDialogButtonBox::accepted, this,
		&ProjectPlaybackDialog::accept);
	connect(
		buttons, &QDialogButtonBox::rejected, this,
		&ProjectPlaybackDialog::reject);

	m_stack->setCurrentWidget(m_messagePage);
	updatePlayState();
}

void ProjectPlaybackDialog::openRecording(
	const QString &basename, const QString &path,
	QTemporaryFile *tempRecordingFile)
{
	if(!m_tempFile.isNull()) {
		qWarning(
			"Project file already set, ignoring request to open recording '%s'",
			qUtf8Printable(path));
		delete tempRecordingFile;
		return;
	}

	m_basename = basename;
	updateTitle();

	m_tempFile.reset(new utils::TempFile);
	if(m_tempFile->setTemporaryPath()) {
		m_messageBar->setRange(0, 100);
		m_messageBar->setValue(0);
		setMessage(tr("Converting recording %1…").arg(basename));

		impex::RecordingConverter *converter =
			new impex::RecordingConverter(path, m_tempFile);

		connect(
			this, &ProjectPlaybackDialog::destroyed, converter,
			&impex::RecordingConverter::cancel, Qt::DirectConnection);
		connect(
			converter, &impex::RecordingConverter::conversionSucceeded, this,
			&ProjectPlaybackDialog::onConversionSucceeded,
			Qt::QueuedConnection);
		connect(
			converter, &impex::RecordingConverter::conversionCancelled, this,
			&ProjectPlaybackDialog::onConversionCancelled,
			Qt::QueuedConnection);
		connect(
			converter, &impex::RecordingConverter::conversionFailed, this,
			&ProjectPlaybackDialog::onConversionFailed, Qt::QueuedConnection);
		connect(
			converter, &impex::RecordingConverter::conversionProgress, this,
			&ProjectPlaybackDialog::setMessageProgress, Qt::QueuedConnection);

		// The temporary file given here has to get deleted once we're done
		// converting it, so just parent it and let it perish naturally.
		if(tempRecordingFile) {
			tempRecordingFile->setParent(converter);
		}

		converter->setAutoDelete(true);
		QThreadPool::globalInstance()->start(converter);

	} else {
		m_messageBar->hide();
		setMessage(tr("Failed to open temporary file."));
	}
}

void ProjectPlaybackDialog::openProject(const QString &path)
{
	if(!m_tempFile.isNull()) {
		qWarning(
			"Project file already set, ignoring request to open project '%s'",
			qUtf8Printable(path));
		return;
	}

	m_tempFile.reset(new utils::TempFile);
	m_tempFile->setPath(path);
	openProjectWrangler();
}

void ProjectPlaybackDialog::setPlaying(bool playing)
{
	// TODO
}

widgets::GroupedToolButton *ProjectPlaybackDialog::makePlaybackButton(
	int groupPosition, const QString &iconName, const QString &tip)
{
	widgets::GroupedToolButton *button = new widgets::GroupedToolButton(
		widgets::GroupedToolButton::GroupPosition(groupPosition));
	button->setToolButtonStyle(Qt::ToolButtonIconOnly);
	button->setIconSize(QSize(22, 22));
	if(!iconName.isEmpty()) {
		button->setIcon(QIcon::fromTheme(iconName));
	}
	if(!tip.isEmpty()) {
		button->setStatusTip(tip);
		button->setToolTip(tip);
	}
	return button;
}

void ProjectPlaybackDialog::updateTitle()
{
	setWindowTitle(QStringLiteral("%1 - %2").arg(
		utils::PathInfo::stripExtension(m_basename),
		QCoreApplication::translate("dialogs::PlaybackDialog", "Playback")));
}

void ProjectPlaybackDialog::setMessage(
	const QString &text, const QString &toolTip)
{
	m_messageLabel->setText(text);
	m_messageLabel->setToolTip(toolTip);
}

void ProjectPlaybackDialog::setMessageProgress(int percent)
{
	m_messageBar->setValue(percent);
}

void ProjectPlaybackDialog::showErrorPage(const QString &errorMessage)
{
	m_messageBar->hide();
	setMessage(errorMessage);
	m_stack->setCurrentWidget(m_messagePage);
}

void ProjectPlaybackDialog::onConversionSucceeded()
{
	m_messageBar->setRange(0, 0);
	openProjectWrangler();
}

void ProjectPlaybackDialog::onConversionCancelled()
{
	m_messageBar->hide();
	setMessage(tr("Conversion cancelled."));
}

void ProjectPlaybackDialog::onConversionFailed(
	const QString &errorMessage, const QString &detail)
{
	m_messageBar->hide();
	setMessage(errorMessage, detail);
}

void ProjectPlaybackDialog::openProjectWrangler()
{
	Q_ASSERT(!m_tempFile.isNull());
	Q_ASSERT(!m_projectWrangler);

	setMessage(tr("Opening project…"));

	m_projectWrangler = new project::ProjectWrangler(this);
	connect(
		m_projectWrangler, &project::ProjectWrangler::openSucceeded, this,
		&ProjectPlaybackDialog::onProjectOpenSucceeded, Qt::QueuedConnection);
	connect(
		m_projectWrangler, &project::ProjectWrangler::playerPrepared, this,
		&ProjectPlaybackDialog::onProjectPlayerPrepared, Qt::QueuedConnection);
	connect(
		m_projectWrangler, &project::ProjectWrangler::errorOccurred, this,
		&ProjectPlaybackDialog::onProjectErrorOccurred, Qt::QueuedConnection);

	m_projectWrangler->openProject(m_tempFile->path());
}

void ProjectPlaybackDialog::onProjectErrorOccurred(
	int type, const QString &errorMessage)
{
	switch(type) {
	case int(project::ProjectWrangler::Error::Open):
	case int(project::ProjectWrangler::Error::PreparePlayer):
		showErrorPage(errorMessage);
		break;
	default:
		ProjectDialog::showUnhandledProjectErrorMessageBoxOn(
			this, errorMessage);
		break;
	}
}

void ProjectPlaybackDialog::onProjectOpenSucceeded()
{
	m_projectWrangler->preparePlayer(
		TIMESTAMP_INDEX_INTERVAL, MAX_DELTA_SECONDS);
}

void ProjectPlaybackDialog::onProjectPlayerPrepared(double totalPlaybackSeconds)
{
	if(totalPlaybackSeconds <= 0.0) {
		showErrorPage(tr("Nothing to play back."));
	} else {
		qWarning("totalPlaybackSeconds %f", totalPlaybackSeconds);
		m_totalPlaybackSeconds = totalPlaybackSeconds;
		m_stack->setCurrentWidget(m_playbackPage);
	}
}

void ProjectPlaybackDialog::onRewindClicked() {}
void ProjectPlaybackDialog::onBackwardClicked() {}
void ProjectPlaybackDialog::onPlayPauseClicked() {}
void ProjectPlaybackDialog::onStepClicked() {}
void ProjectPlaybackDialog::onSkipClicked() {}

void ProjectPlaybackDialog::onProgressValueChanged(int value) {}
void ProjectPlaybackDialog::onProgressSliderReleased() {}

void ProjectPlaybackDialog::updatePlayState()
{
	if(m_playing) {
		m_playPauseButton->setIcon(m_pauseIcon);
		m_playPauseButton->setStatusTip(m_pauseTip);
		m_playPauseButton->setToolTip(m_pauseTip);
	} else {
		m_playPauseButton->setIcon(m_playIcon);
		m_playPauseButton->setStatusTip(m_playTip);
		m_playPauseButton->setToolTip(m_playTip);
	}
	m_rewindButton->setEnabled(!m_playing);
	m_backwardButton->setEnabled(!m_playing);
	m_playPauseButton->setEnabled(true);
	m_stepButton->setEnabled(!m_playing);
	m_skipButton->setEnabled(!m_playing);
}

}
