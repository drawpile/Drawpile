// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_TIMELAPSEDIALOG_H
#define DESKTOP_DIALOGS_TIMELAPSEDIALOG_H
#include "libclient/drawdance/canvasstate.h"
#include <QDialog>
#include <QImage>

class KisDoubleSliderSpinBox;
class KisSliderSpinBox;
class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QHBoxLayout;
class QLabel;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QTimeEdit;
class TimelapseSaverRunnable;

namespace color_widgets {
class ColorPreview;
}

namespace utils {
class FormNote;
}

namespace widgets {
class TimelapsePreview;
}

namespace dialogs {

class TimelapseDialog final : public QDialog {
	Q_OBJECT
public:
	explicit TimelapseDialog(
		const drawdance::CanvasState &canvasState, QWidget *parent = nullptr);

	void setInputPath(const QString &inputPath) { m_inputPath = inputPath; }

public slots:
	void accept() override;
	void reject() override;

private:
	static constexpr int DEFAULT_MAX_WIDTH = 1920;
	static constexpr int DEFAULT_MAX_HEIGHT = 1920;
	static constexpr int DEFAULT_FALLBACK_WIDTH = 1920;
	static constexpr int DEFAULT_FALLBACK_HEIGHT = 1080;

	enum class LogoLocation {
		TopLeft,
		TopRight,
		BottomLeft,
		BottomRight,
		Default = BottomRight,
	};

	void setDefaultResolution();
	void resetToDefaultSettings();
	void resetDefaultExportFormat();
	void loadSettings();
	void saveSettings();
	bool selectExportFormat(int format);
	bool selectInterpolation(int interpolation);
	bool selectLogoLocation(int location);
	int getDurationSeconds() const;
	void setDurationSeconds(int seconds);
	void updateWidth(int value);
	void updateHeight(int value);
	void updateAspectRatio(bool checked);
	void updateLogoRect();
	void updateLogoOpacity(int opacity);
	void updatePreviewSize();
	void updateDimensionsNote();
	void toggleAdvanced();
	void updateAdvanced(bool enabled);
	void updateFramerateNote();

	QSize getOutputSize() const;
	QRect getLogoRect();
	const QImage &getLogoImage();

	void pickFlashColor();
	void confirmReset();
	QString choosePath(int format);

	void showSettingsPage();
	void showProgressPage();
	void showFinishPage();

	void updateProgressLabel(const QString &message);
	void setProgress(int percent);
	void handleSaveComplete(qint64 msecs);
	void handleSaveCancelled();
	void handleSaveFailed(const QString &errorMessage);

	drawdance::CanvasState m_canvasState;
	QString m_inputPath;
	QScrollArea *m_scroll;
	widgets::TimelapsePreview *m_timelapsePreview;
	QWidget *m_settingsPage;
	QWidget *m_progressPage;
	QWidget *m_finishPage;
	QComboBox *m_formatCombo;
	QTimeEdit *m_durationEdit;
	QSpinBox *m_widthSpinner;
	QSpinBox *m_heightSpinner;
	QCheckBox *m_keepAspectCheckBox;
	QCheckBox *m_showLogoCheckBox;
	utils::FormNote *m_dimensionsNote;
	QPushButton *m_advancedButton;
	QWidget *m_advancedWidget;
	QComboBox *m_interpolationCombo;
	QComboBox *m_logoLocationCombo;
	KisDoubleSliderSpinBox *m_logoScaleSlider;
	KisDoubleSliderSpinBox *m_logoOffsetSlider;
	KisSliderSpinBox *m_logoOpacitySlider;
	KisDoubleSliderSpinBox *m_lingerBeforeSlider;
	color_widgets::ColorPreview *m_flashPreview;
	KisDoubleSliderSpinBox *m_flashSlider;
	KisDoubleSliderSpinBox *m_lingerAfterSlider;
	KisDoubleSliderSpinBox *m_maxDeltaSlider;
	KisSliderSpinBox *m_maxQueueEntriesSlider;
	KisDoubleSliderSpinBox *m_framerateSlider;
	utils::FormNote *m_framerateNote;
	QLabel *m_progressLabel;
	QProgressBar *m_progressBar;
	QLabel *m_finishLabel;
	QDialogButtonBox *m_buttons;
	TimelapseSaverRunnable *m_saver = nullptr;
	QImage m_logoImage;
	qreal m_aspectRatio = 0.0;
	bool m_cancelling = false;
};

}

#endif
