// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/projectplaybackdialog.h"
#include "desktop/dialogs/projectdialog.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/noscroll.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/import/recordingconverter.h"
#include "libclient/project/projectwrangler.h"
#include "libclient/utils/pathinfo.h"
#include "libclient/utils/qtguicompat.h"
#include "libclient/utils/strings.h"
#include "libclient/utils/tempfile.h"
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QTemporaryFile>
#include <QThreadPool>
#include <QTimer>
#include <QVBoxLayout>

namespace dialogs {

class ProjectPlaybackDialog::PlaybackSlider : public widgets::NoScrollSlider {
public:
	explicit PlaybackSlider(QWidget *parent = nullptr)
		: widgets::NoScrollSlider(parent)
	{
		setOrientation(Qt::Horizontal);
		setRange(0, 10000);
		setPageStep(1000);
		setSingleStep(100);
	}

	int trackValue() const { return m_trackValue; }
	void setTrackValue(int trackValue)
	{
		int effectiveTrackValue = qBound(minimum(), trackValue, maximum());
		if(effectiveTrackValue != m_trackValue) {
			m_trackValue = effectiveTrackValue;
			update();
		}
	}

	void updateValues(int value) { updateValuesSeparate(value, value); }

	void updateValuesSeparate(int value, int trackValue)
	{
		QSignalBlocker blocker(this);
		setValue(value);
		setTrackValue(trackValue);
	}

	double toSeconds(double totalPlaybackSeconds, int value) const
	{
		int min = minimum();
		int max = maximum();
		double ratio = double(value - min) / double(max - min);
		return qBound(0.0, totalPlaybackSeconds * ratio, totalPlaybackSeconds);
	}

protected:
	void paintEvent(QPaintEvent *event) override
	{
		Q_UNUSED(event);

		QPainter painter(this);
		QStyleOptionSlider opt;
		initStyleOption(&opt);

		int actualValue = opt.sliderValue;
		int actualSliderPosition = opt.sliderPosition;

		opt.sliderValue = m_trackValue;
		opt.sliderPosition = m_trackValue;
		opt.subControls = QStyle::SC_SliderGroove;
		if(tickPosition() != NoTicks) {
			opt.subControls |= QStyle::SC_SliderTickmarks;
		}
		style()->drawComplexControl(QStyle::CC_Slider, &opt, &painter, this);

		opt.sliderValue = actualValue;
		opt.sliderPosition = actualSliderPosition;
		opt.subControls = QStyle::SC_SliderHandle;
		style()->drawComplexControl(QStyle::CC_Slider, &opt, &painter, this);
	}

	// The mouse and keyboard behavior of a regular QSlider isn't conducive to
	// this kind of slow seek bar. It also bugs out really easily at the right
	// end, not triggering a slider release event. We do it ourselves instead.

	void mousePressEvent(QMouseEvent *event) override
	{
		if(event->button() == Qt::LeftButton) {
			m_mouseDown = true;
			QStyleOptionSlider opt;
			initStyleOption(&opt);

			QRect handleRect = getSliderRect(opt, QStyle::SC_SliderHandle);
			QPoint mousePos = compat::mousePos(*event);
			m_dragRelative = handleRect.contains(mousePos);
			if(m_dragRelative) {
				m_pressOffset = mousePos.x() - handleRect.center().x();
			}

			moveSliderByMouseWith(opt, event);
			event->accept();
			Q_EMIT sliderPressed();
		}
	}

	void mouseDoubleClickEvent(QMouseEvent *event) override
	{
		mousePressEvent(event);
	}

	void mouseMoveEvent(QMouseEvent *event) override
	{
		if(m_mouseDown) {
			moveSliderByMouse(event);
			event->accept();
		}
	}

	void mouseReleaseEvent(QMouseEvent *event) override
	{
		if(m_mouseDown) {
			moveSliderByMouse(event);
			event->accept();
			if(event->button() == Qt::LeftButton) {
				m_mouseDown = false;
				Q_EMIT sliderReleased();
			}
		}
	}

	void keyPressEvent(QKeyEvent *event) override
	{
		QWidget::keyPressEvent(event);
	}

	void keyReleaseEvent(QKeyEvent *event) override
	{
		QWidget::keyReleaseEvent(event);
	}

private:
	void moveSliderByMouse(QMouseEvent *event)
	{
		QStyleOptionSlider opt;
		initStyleOption(&opt);
		moveSliderByMouseWith(opt, event);
	}

	void
	moveSliderByMouseWith(const QStyleOptionSlider &opt, QMouseEvent *event)
	{
		int mouseX = compat::mousePos(*event).x();
		if(m_dragRelative) {
			QRect handleRect = getSliderRect(opt, QStyle::SC_SliderHandle);
			QRect grooveRect = getSliderRect(opt, QStyle::SC_SliderGroove);
			int handleWidth = handleRect.width();
			int grooveWidth = grooveRect.width() - handleWidth;
			if(grooveWidth > 0) {
				int targetPos = mouseX - m_pressOffset - grooveRect.left() -
								(handleWidth / 2);
				double ratio = double(targetPos) / double(grooveWidth);
				int min = minimum();
				int max = maximum();
				setValue(min + qRound(ratio * double(max - min)));
			}
		} else {
			setValue(
				QStyle::sliderValueFromPosition(
					minimum(), maximum(), mouseX, width(), opt.upsideDown));
		}
	}

	QRect getSliderRect(
		const QStyleOptionSlider &opt, QStyle::SubControl subControl) const
	{
		return style()->subControlRect(
			QStyle::CC_Slider, &opt, subControl, this);
	}

	int m_trackValue = 0;
	int m_pressOffset = 0;
	bool m_mouseDown = false;
	bool m_dragRelative = false;
};

ProjectPlaybackDialog::ProjectPlaybackDialog(QWidget *parent)
	: QDialog(parent)
	, m_playTip(tr("Play"))
	, m_pauseTip(tr("Pause"))
	, m_playIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")))
	, m_pauseIcon(QIcon::fromTheme(QStringLiteral("media-playback-pause")))
	, m_playerState(int(project::ProjectWrangler::PlayerState::Ok))
{
	setWindowFlags(Qt::Tool);
	resize(420, 100);

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

	m_previousSessionButton = makePlaybackButton(
		int(widgets::GroupedToolButton::GroupLeft), QStringLiteral("go-first"),
		tr("Rewind session"));
	buttonsLayout->addWidget(m_previousSessionButton, 1);
	connect(
		m_previousSessionButton, &widgets::GroupedToolButton::clicked, this,
		&ProjectPlaybackDialog::onPreviousSessionClicked);

	m_playPauseButton = makePlaybackButton(
		int(widgets::GroupedToolButton::GroupCenter), QString(), QString());
	buttonsLayout->addWidget(m_playPauseButton, 2);
	connect(
		m_playPauseButton, &widgets::GroupedToolButton::clicked, this,
		&ProjectPlaybackDialog::onPlayPauseClicked);

	m_nextStrokeButton = makePlaybackButton(
		int(widgets::GroupedToolButton::GroupCenter), QStringLiteral("go-next"),
		tr("Skip stroke"));
	buttonsLayout->addWidget(m_nextStrokeButton, 1);
	connect(
		m_nextStrokeButton, &widgets::GroupedToolButton::clicked, this,
		&ProjectPlaybackDialog::onNextStrokeClicked);

	m_nextSessionButton = makePlaybackButton(
		int(widgets::GroupedToolButton::GroupRight), QStringLiteral("go-last"),
		tr("Skip session"));
	buttonsLayout->addWidget(m_nextSessionButton, 1);
	connect(
		m_nextSessionButton, &widgets::GroupedToolButton::clicked, this,
		&ProjectPlaybackDialog::onNextSessionClicked);

	buttonsLayout->addStretch(1);

	m_playbackSpeedSlider = new widgets::NoScrollKisSliderSpinBox;
	m_playbackSpeedSlider->setRange(1, 10000);
	m_playbackSpeedSlider->setExponentRatio(3.0);
	m_playbackSpeedSlider->setValue(100);
	m_playbackSpeedSlider->setPrefix(tr("Playback speed: "));
	m_playbackSpeedSlider->setSuffix(strings::percent());
	playbackLayout->addWidget(m_playbackSpeedSlider);
	connect(
		m_playbackSpeedSlider,
		QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&ProjectPlaybackDialog::onPlaybackSpeedSliderValueChanged);

	m_progressSlider = new PlaybackSlider;
	playbackLayout->addWidget(m_progressSlider);
	connect(
		m_progressSlider, QOverload<int>::of(&QSlider::valueChanged), this,
		&ProjectPlaybackDialog::onProgressValueChanged);
	connect(
		m_progressSlider, &QSlider::sliderReleased, this,
		&ProjectPlaybackDialog::onProgressSliderReleased);

	m_progressStack = new QStackedWidget;
	playbackLayout->addWidget(m_progressStack);

	m_progressLabel = new QLabel;
	m_progressLabel->setAlignment(Qt::AlignCenter);
	m_progressStack->addWidget(m_progressLabel);

	m_progressCancel = new QWidget;
	m_progressCancel->setContentsMargins(0, 0, 0, 0);
	m_progressStack->addWidget(m_progressCancel);

	QHBoxLayout *progressCancelLayout = new QHBoxLayout(m_progressCancel);
	progressCancelLayout->setContentsMargins(0, 0, 0, 0);
	progressCancelLayout->setSpacing(0);

	progressCancelLayout->addStretch(1);
	QPushButton *progressCancelButton = new QPushButton(
		QCoreApplication::translate("QPlatformTheme", "Cancel"));
	progressCancelLayout->addWidget(progressCancelButton);
	connect(
		progressCancelButton, &QPushButton::clicked, this,
		&ProjectPlaybackDialog::triggerCancel);
	progressCancelLayout->addStretch(1);

	playbackLayout->addStretch(1);

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

ProjectPlaybackDialog::~ProjectPlaybackDialog()
{
	setPlaying(false);
	if(m_projectWrangler) {
		m_projectWrangler->cancelPlayer();
	}
}

void ProjectPlaybackDialog::openProject(
	canvas::PaintEngine *paintEngine, const QString &basename,
	const QString &path, QTemporaryFile *tempFile)
{
	if(m_tempFileHolder || m_projectWrangler) {
		qWarning(
			"Project file already set, ignoring request to open project '%s'",
			qUtf8Printable(path));
		return;
	}

	m_paintEngine = paintEngine;
	m_basename = basename;
	updateTitle();

	openProjectWrangler(path, !tempFile);

	if(tempFile) {
		tempFile->setParent(m_projectWrangler);
	}
}


void ProjectPlaybackDialog::openRecording(
	canvas::PaintEngine *paintEngine, const QString &basename,
	const QString &path, QTemporaryFile *tempRecordingFile)
{
	if(m_tempFileHolder || m_projectWrangler) {
		qWarning(
			"Project file already set, ignoring request to open recording '%s'",
			qUtf8Printable(path));
		delete tempRecordingFile;
		return;
	}

	m_paintEngine = paintEngine;
	m_basename = basename;
	updateTitle();

	m_tempFileHolder = new utils::TempFileHolder(new utils::TempFile);
	if(m_tempFileHolder->setTemporaryPath()) {
		m_messageBar->setRange(0, 100);
		m_messageBar->setValue(0);
		setMessage(tr("Converting recording %1…").arg(basename));

		impex::RecordingConverter *converter = new impex::RecordingConverter(
			path, m_tempFileHolder->sharedPointer());

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

void ProjectPlaybackDialog::setPlaying(bool playing)
{
	if(playing) {
		triggerPlay();
	} else if(!playing) {
		triggerPause();
	}
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
	openProjectWrangler(m_tempFileHolder->path(), false);
	m_tempFileHolder->setParent(m_projectWrangler);
	m_tempFileHolder = nullptr;
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

void ProjectPlaybackDialog::openProjectWrangler(
	const QString &path, bool copyToTemporary)
{
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
	connect(
		m_projectWrangler, &project::ProjectWrangler::playerProgressed, this,
		&ProjectPlaybackDialog::onProjectPlayerProgressed,
		Qt::QueuedConnection);
	connect(
		m_projectWrangler, &project::ProjectWrangler::playerUpdated, this,
		&ProjectPlaybackDialog::onProjectPlayerUpdated, Qt::QueuedConnection);
	connect(
		m_projectWrangler, &project::ProjectWrangler::playerControlCompleted,
		this, &ProjectPlaybackDialog::onProjectPlayerControlCompleted,
		Qt::QueuedConnection);

	onPlaybackSpeedSliderValueChanged(m_playbackSpeedSlider->value());
	m_projectWrangler->openProject(path, false, copyToTemporary);
}

void ProjectPlaybackDialog::onProjectErrorOccurred(
	int type, const QString &errorMessage)
{
	switch(type) {
	case int(project::ProjectWrangler::Error::Open):
	case int(project::ProjectWrangler::Error::PreparePlayer):
		showErrorPage(errorMessage);
		break;
	case int(project::ProjectWrangler::Error::ControlPlayer):
		utils::showWarning(this, tr("Player Error"), errorMessage);
		break;
	default:
		ProjectDialog::showUnhandledProjectErrorMessageBoxOn(
			this, errorMessage);
		break;
	}
}

void ProjectPlaybackDialog::onProjectOpenSucceeded()
{
	m_projectWrangler->preparePlayer(MAX_DELTA_SECONDS, SNAPSHOT_INTERVAL);
}

void ProjectPlaybackDialog::onProjectPlayerPrepared(double totalPlaybackSeconds)
{
	if(totalPlaybackSeconds <= 0.0) {
		showErrorPage(tr("Nothing to play back."));
	} else {
		m_totalPlaybackSeconds = totalPlaybackSeconds;
		m_state = State::Paused;
		m_stack->setCurrentWidget(m_playbackPage);
		m_progressSlider->updateValues(m_progressSlider->minimum());
		updatePlayState();
		updateProgressLabelText();
		triggerRewind();
	}
}

void ProjectPlaybackDialog::onProjectPlayerProgressed(
	unsigned int controlId, int playerState, double playbackSeconds,
	long long sessionId, long long sequenceId)
{
	if(controlId == m_controlId) {
		updatePlayer(playerState, playbackSeconds, sessionId, sequenceId);
	} else {
		qWarning(
			"Got project player update for control id %u when expecting %u",
			controlId, m_controlId);
	}
}

void ProjectPlaybackDialog::onProjectPlayerUpdated(
	unsigned int controlId, int playerState,
	const drawdance::CanvasState &canvasState, double playbackSeconds,
	long long sessionId, long long sequenceId)
{
	if(controlId == m_controlId) {
		m_paintEngine->enqueueResetToState(canvasState);
		updatePlayer(playerState, playbackSeconds, sessionId, sequenceId);
	} else {
		qWarning(
			"Got project player update for control id %u when expecting %u",
			controlId, m_controlId);
	}
}

void ProjectPlaybackDialog::updatePlayer(
	int playerState, double playbackSeconds, long long sessionId,
	long long sequenceId)
{
	m_playerState = playerState;
	m_currentPlaybackSeconds = playbackSeconds;
	m_currentPlaybackSessionId = sessionId;
	m_currentPlaybackSequenceId = sequenceId;

	if(playerState == int(project::ProjectWrangler::PlayerState::End)) {
		m_progressSlider->updateValues(m_progressSlider->maximum());
	} else if(m_totalPlaybackSeconds > 0.0) {
		double ratio = (playbackSeconds / m_totalPlaybackSeconds);
		double min = double(m_progressSlider->minimum());
		double max = double(m_progressSlider->maximum());
		int trackValue = qRound(min + (ratio * (max - min)));
		if(m_state == State::Seeking) {
			m_progressSlider->setTrackValue(trackValue);
		} else {
			m_progressSlider->updateValues(trackValue);
		}
	}

	updateProgressLabelText();
}

void ProjectPlaybackDialog::onProjectPlayerControlCompleted(
	unsigned int controlId)
{
	if(m_state != State::NotPrepared && controlId == m_controlId) {
		m_state = State::Paused;
		m_progressSlider->updateValues(m_progressSlider->trackValue());
		updatePlayState();
	}
}

void ProjectPlaybackDialog::onPreviousSessionClicked()
{
	triggerStepSessions(-1);
}

void ProjectPlaybackDialog::onPlayPauseClicked()
{
	if(m_state == State::Playing) {
		triggerPause();
	} else if(m_state == State::Paused) {
		triggerPlay();
	}
}

void ProjectPlaybackDialog::onNextStrokeClicked()
{
	triggerStepUndoPoints(1);
}

void ProjectPlaybackDialog::onNextSessionClicked()
{
	triggerStepSessions(1);
}

void ProjectPlaybackDialog::onPlaybackSpeedSliderValueChanged(int value)
{
	bool uncapped = value >= m_playbackSpeedSlider->maximum();
	if(uncapped) {
		m_playbackSpeedSlider->setOverrideText(
			//: This refers to uncapped playback speed. I didn't want to call
			//: this "unlimited" because it's still limited by how fast the
			//: device can actually play a recording, but no artificial limits.
			m_playbackSpeedSlider->prefix() + tr("uncapped"));
	} else {
		m_playbackSpeedSlider->setOverrideText(QString());
	}

	if(m_projectWrangler) {
		m_projectWrangler->setPlaybackSpeed(uncapped ? 0.0 : double(value));
	}
}

void ProjectPlaybackDialog::onProgressValueChanged(int value)
{
	if(isPaused()) {
		setProgressLabelText(
			m_progressSlider->toSeconds(m_totalPlaybackSeconds, value));
	}
}

void ProjectPlaybackDialog::onProgressSliderReleased()
{
	if(isPaused() &&
	   m_progressSlider->value() != m_progressSlider->trackValue()) {
		updateProgressLabelText();
		int value = m_progressSlider->value();
		if(value <= m_progressSlider->minimum()) {
			triggerRewind();
		} else if(value >= m_progressSlider->maximum()) {
			triggerFastForward();
		} else {
			triggerSeek(
				m_progressSlider->toSeconds(m_totalPlaybackSeconds, value));
		}
	}
}

void ProjectPlaybackDialog::updatePlayState()
{
	bool playing, available;
	switch(m_state) {
	case State::Paused:
		playing = false;
		available = true;
		break;
	case State::Playing:
		playing = true;
		available = false;
		break;
	default:
		playing = false;
		available = false;
		break;
	}

	bool end, error;
	switch(m_playerState) {
	case int(project::ProjectWrangler::PlayerState::End):
		end = true;
		error = false;
		break;
	case int(project::ProjectWrangler::PlayerState::Error):
		end = false;
		error = true;
		break;
	default:
		end = false;
		error = false;
		break;
	}

	if(playing || m_state == State::Pausing) {
		m_playPauseButton->setIcon(m_pauseIcon);
		m_playPauseButton->setStatusTip(m_pauseTip);
		m_playPauseButton->setToolTip(m_pauseTip);
	} else {
		m_playPauseButton->setIcon(m_playIcon);
		m_playPauseButton->setStatusTip(m_playTip);
		m_playPauseButton->setToolTip(m_playTip);
	}

	m_previousSessionButton->setEnabled(
		available && m_currentPlaybackSeconds != 0.0);
	m_playPauseButton->setEnabled((playing || available) && !end && !error);
	m_nextStrokeButton->setEnabled(available && !end && !error);
	m_nextSessionButton->setEnabled(available && !end && !error);
	m_playbackSpeedSlider->setEnabled(playing || available);
	m_progressSlider->setEnabled(available);

	QWidget *progressPage;
	switch(m_state) {
	case State::Rewinding:
	case State::FastForwarding:
	case State::SteppingSessions:
	case State::SteppingUndoPoints:
	case State::Seeking:
		progressPage = m_progressCancel;
		break;
	default:
		progressPage = m_progressLabel;
		break;
	}
	m_progressStack->setCurrentWidget(progressPage);
}

void ProjectPlaybackDialog::updateProgressLabelText()
{
	setProgressLabelText(m_currentPlaybackSeconds);
}

void ProjectPlaybackDialog::setProgressLabelText(double seconds)
{
	QString sessionText;
	switch(m_playerState) {
	case int(project::ProjectWrangler::PlayerState::End):
		sessionText = tr("Session %1 - End");
		break;
	case int(project::ProjectWrangler::PlayerState::Error):
		sessionText = tr("Session %1 - Error");
		break;
	default:
		sessionText = tr("Session %1");
		break;
	}
	m_progressLabel->setText(
		QStringLiteral("%1 - %2 / %3")
			.arg(
				sessionText.arg(m_currentPlaybackSessionId),
				formatProgressTime(seconds),
				formatProgressTime(m_totalPlaybackSeconds)));
	m_progressLabel->setToolTip(
		tr("Sequence number %1").arg(m_currentPlaybackSequenceId));
}

void ProjectPlaybackDialog::triggerRewind()
{
	if(isPaused()) {
		m_state = State::Rewinding;
		m_controlId = m_projectWrangler->rewindPlayer();
		updatePlayState();
	}
}

void ProjectPlaybackDialog::triggerFastForward()
{
	if(isPaused()) {
		m_state = State::FastForwarding;
		m_controlId = m_projectWrangler->fastForwardPlayer();
		updatePlayState();
	}
}

void ProjectPlaybackDialog::triggerStepSessions(int delta)
{
	if(delta != 0 && isPaused()) {
		m_state = State::SteppingSessions;
		m_controlId = m_projectWrangler->skipPlayerSessions(delta);
		updatePlayState();
	}
}

void ProjectPlaybackDialog::triggerStepUndoPoints(int undoPointCount)
{
	if(isPaused() && undoPointCount > 0) {
		m_state = State::SteppingUndoPoints;
		m_controlId = m_projectWrangler->stepPlayerUndoPoints(undoPointCount);
		updatePlayState();
	}
}

void ProjectPlaybackDialog::triggerPlay()
{
	if(isPaused()) {
		m_state = State::Playing;
		m_controlId = m_projectWrangler->startPlayer();
		updatePlayState();
	}
}

void ProjectPlaybackDialog::triggerPause()
{
	if(isPlaying()) {
		m_state = State::Pausing;
		m_projectWrangler->pausePlayer();
		updatePlayState();
	}
}

void ProjectPlaybackDialog::triggerSeek(double seconds)
{
	if(isPaused()) {
		m_state = State::Seeking;
		m_controlId = m_projectWrangler->seekPlayer(seconds);
		updatePlayState();
	}
}

void ProjectPlaybackDialog::triggerCancel()
{
	if(m_projectWrangler) {
		m_projectWrangler->cancelPlayer();
	}
}

QString ProjectPlaybackDialog::formatProgressTime(double seconds) const
{
	QString s;
	int secondsLeft = int(seconds);

	if(m_totalPlaybackSeconds >= 3600.0) {
		int total = int(m_totalPlaybackSeconds) / 3600;
		int digits = 0;
		while(total > 0) {
			total /= 10;
			++digits;
		}

		int hours = secondsLeft / 3600;
		secondsLeft -= hours * 3600;
		s.append(QStringLiteral("%1:").arg(hours, digits, 10, QChar('0')));
	}

	if(m_totalPlaybackSeconds >= 60.0) {
		int minutes = secondsLeft / 60;
		secondsLeft -= minutes * 60;
		s.append(QStringLiteral("%1:").arg(minutes, 2, 10, QChar('0')));
	}

	s.append(QStringLiteral("%1.%2")
				 .arg(secondsLeft, 2, 10, QChar('0'))
				 .arg(qRound(seconds * 1000.0) % 1000, 3, 10, QChar('0')));
	return s;
}

}
