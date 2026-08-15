// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_PROJECTPLAYBACKDIALOG_H
#define DESKTOP_DIALOGS_PROJECTPLAYBACKDIALOG_H
#include "libclient/drawdance/canvasstate.h"
#include "libclient/net/message.h"
#include <QDialog>
#include <QIcon>

class KisSliderSpinBox;
class QLabel;
class QProgressBar;
class QStackedWidget;
class QTemporaryFile;

namespace canvas {
class PaintEngine;
}

namespace project {
class ProjectWrangler;
}

namespace utils {
class TempFileHolder;
}

namespace widgets {
class GroupedToolButton;
}

namespace dialogs {

class ProjectPlaybackDialog final : public QDialog {
	Q_OBJECT
public:
	explicit ProjectPlaybackDialog(QWidget *parent = nullptr);
	~ProjectPlaybackDialog() override;

	void openProject(
		canvas::PaintEngine *paintEngine, const QString &basename,
		const QString &path, QTemporaryFile *tempFile = nullptr);
	void openRecording(
		canvas::PaintEngine *paintEngine, const QString &basename,
		const QString &path, QTemporaryFile *tempRecordingFile = nullptr);

	bool isPlaying() const { return m_state == State::Playing; }
	void setPlaying(bool playing);

private:
	class PlaybackSlider;

	enum class State {
		NotPrepared,
		Paused,
		Playing,
		Pausing,
		Rewinding,
		FastForwarding,
		SteppingSessions,
		SteppingUndoPoints,
		Seeking,
	};

	static constexpr double MAX_DELTA_SECONDS = 0.1;
	static constexpr long long SNAPSHOT_INTERVAL = 8192LL;

	static widgets::GroupedToolButton *makePlaybackButton(
		int groupPosition, const QString &iconName, const QString &tip);

	bool isPaused() const { return m_state == State::Paused; }

	void updateTitle();

	void setMessage(const QString &text, const QString &toolTip = QString());
	void setMessageProgress(int percent);
	void showErrorPage(const QString &errorMessage);

	void onConversionSucceeded();
	void onConversionCancelled();
	void onConversionFailed(const QString &errorMessage, const QString &detail);

	void openProjectWrangler(const QString &path, bool copyToTemporary);
	void onProjectErrorOccurred(int type, const QString &errorMessage);
	void onProjectOpenSucceeded();
	void onProjectPlayerPrepared(double totalPlaybackSeconds);
	void onProjectPlayerProgressed(
		unsigned int controlId, int playerState, double playbackSeconds,
		long long sessionId, long long sequenceId);
	void onProjectPlayerUpdated(
		unsigned int controlId, int playerState,
		const drawdance::CanvasState &canvasState, double playbackSeconds,
		long long sessionId, long long sequenceId, bool localStateChanged,
		const net::MessageList &localStateMsgs, bool viewStateChanged,
		QSize viewportSize, QPointF pos, qreal zoom, qreal rotation,
		bool mirror, bool flip);
	void updatePlayer(
		int playerState, double playbackSeconds, long long sessionId,
		long long sequenceId);
	void onProjectPlayerControlCompleted(unsigned int controlId);

	void onPreviousSessionClicked();
	void onPlayPauseClicked();
	void onNextStrokeClicked();
	void onNextSessionClicked();

	void onPlaybackSpeedSliderValueChanged(int value);
	void onProgressValueChanged(int value);
	void onProgressSliderReleased();

	void updatePlayState();
	void updateProgressLabelText();
	void setProgressLabelText(double seconds);

	void triggerRewind();
	void triggerFastForward();
	void triggerStepSessions(int delta);
	void triggerStepUndoPoints(int undoPointCount);
	void triggerPlay();
	void triggerPause();
	void triggerSeek(double seconds);
	void triggerCancel();

	QString formatProgressTime(double seconds) const;

	canvas::PaintEngine *m_paintEngine = nullptr;
	QString m_basename;
	QString m_playTip;
	QString m_pauseTip;
	QIcon m_playIcon;
	QIcon m_pauseIcon;
	utils::TempFileHolder *m_tempFileHolder = nullptr;
	project::ProjectWrangler *m_projectWrangler = nullptr;
	double m_currentPlaybackSeconds = 0.0;
	double m_totalPlaybackSeconds = 0.0;
	long long m_currentPlaybackSessionId = 0LL;
	long long m_currentPlaybackSequenceId = 0LL;
	QStackedWidget *m_stack;
	QWidget *m_messagePage;
	QLabel *m_messageLabel;
	QProgressBar *m_messageBar;
	QWidget *m_playbackPage;
	widgets::GroupedToolButton *m_previousSessionButton;
	widgets::GroupedToolButton *m_playPauseButton;
	widgets::GroupedToolButton *m_nextStrokeButton;
	widgets::GroupedToolButton *m_nextSessionButton;
	KisSliderSpinBox *m_playbackSpeedSlider;
	PlaybackSlider *m_progressSlider;
	QStackedWidget *m_progressStack;
	QLabel *m_progressLabel;
	QWidget *m_progressCancel;
	State m_state = State::NotPrepared;
	int m_playerState;
	unsigned int m_controlId = 0u;
};

}

#endif
