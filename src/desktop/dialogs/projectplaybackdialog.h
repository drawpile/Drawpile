// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_PROJECTPLAYBACKDIALOG_H
#define DESKTOP_DIALOGS_PROJECTPLAYBACKDIALOG_H
#include "libclient/utils/tempfile.h"
#include <QDialog>
#include <QIcon>
#include <QSharedPointer>

class KisSliderSpinBox;
class QLabel;
class QProgressBar;
class QSlider;
class QStackedWidget;
class QTemporaryFile;

namespace project {
class ProjectWrangler;
}

namespace widgets {
class GroupedToolButton;
}

namespace dialogs {

class ProjectPlaybackDialog : public QDialog {
	Q_OBJECT
public:
	explicit ProjectPlaybackDialog(QWidget *parent = nullptr);

	void openProject(const QString &path);
	void openRecording(
		const QString &basename, const QString &path,
		QTemporaryFile *tempRecordingFile = nullptr);

	bool isPlaying() const { return m_playing; }
	void setPlaying(bool playing);

private:
	static constexpr int TIMESTAMP_INDEX_INTERVAL = 4096;
	static constexpr double MAX_DELTA_SECONDS = 0.1;

	static widgets::GroupedToolButton *makePlaybackButton(
		int groupPosition, const QString &iconName, const QString &tip);

	void updateTitle();

	void setMessage(const QString &text, const QString &toolTip = QString());
	void setMessageProgress(int percent);
	void showErrorPage(const QString &errorMessage);

	void onConversionSucceeded();
	void onConversionCancelled();
	void onConversionFailed(const QString &errorMessage, const QString &detail);

	void openProjectWrangler();
	void onProjectErrorOccurred(int type, const QString &errorMessage);
	void onProjectOpenSucceeded();
	void onProjectPlayerPrepared(double totalPlaybackSeconds);

	void onRewindClicked();
	void onBackwardClicked();
	void onPlayPauseClicked();
	void onStepClicked();
	void onSkipClicked();

	void onProgressValueChanged(int value);
	void onProgressSliderReleased();

	void updatePlayState();

	QString m_basename;
	QString m_playTip;
	QString m_pauseTip;
	QIcon m_playIcon;
	QIcon m_pauseIcon;
	QSharedPointer<utils::TempFile> m_tempFile;
	project::ProjectWrangler *m_projectWrangler = nullptr;
	double m_totalPlaybackSeconds = 0.0;
	QStackedWidget *m_stack;
	QWidget *m_messagePage;
	QLabel *m_messageLabel;
	QProgressBar *m_messageBar;
	QWidget *m_playbackPage;
	widgets::GroupedToolButton *m_rewindButton;
	widgets::GroupedToolButton *m_backwardButton;
	widgets::GroupedToolButton *m_playPauseButton;
	widgets::GroupedToolButton *m_stepButton;
	widgets::GroupedToolButton *m_skipButton;
	KisSliderSpinBox *m_playbackSpeedSlider;
	QSlider *m_progressSlider;
	QLabel *m_progressLabel;
	bool m_playing = false;
};

}

#endif
