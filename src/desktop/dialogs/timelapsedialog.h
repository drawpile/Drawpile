// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_TIMELAPSEDIALOG_H
#define DESKTOP_DIALOGS_TIMELAPSEDIALOG_H
#include "libclient/drawdance/canvasstate.h"
#include "libclient/drawdance/viewmode.h"
#include <QDialog>
#include <QImage>
#include <QRect>

class KisDoubleSliderSpinBox;
class KisSliderSpinBox;
class QButtonGroup;
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

namespace canvas {
class PaintEngine;
}

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
		canvas::PaintEngine *paintEngine, const QRect &crop, bool inFrameView,
		double flipbookSpeedPercent, int flipbookFrameRangeFirst,
		int flipbookFrameRangeLast, QWidget *parent = nullptr);

	void setInputPath(const QString &inputPath);
	bool haveInputPath() const { return !m_inputPath.isEmpty(); }

public slots:
	void accept() override;
	void reject() override;

private:
	static constexpr int DEFAULT_MAX_WIDTH = 1920;
	static constexpr int DEFAULT_MAX_HEIGHT = 1920;
	static constexpr int DEFAULT_FALLBACK_WIDTH = 1920;
	static constexpr int DEFAULT_FALLBACK_HEIGHT = 1080;

	enum class LogoLocation {
		None,
		TopLeft,
		TopRight,
		BottomLeft,
		BottomRight,
		Default = BottomRight,
	};

	void setDefaultResolutions();
	QSize calculateResolution(const QSize &inputSize);
	void updateCurrentResolution();
	void updateAnimation();
	void setUseCrop(bool checked);
	void resetToDefaultSettings();
	void resetDefaultExportFormat();
	void loadSettings();
	void saveSettings();
	bool selectExportFormat(int format);
	bool checkLogoLocation(int location);
	bool selectInterpolation(int interpolation);
	int getDurationSeconds() const;
	void setDurationSeconds(int seconds);
	void updateFfmpeg();
	void updateWidth(int value);
	void updateHeight(int value);
	void updateAspectRatio(bool checked);
	void updateLogoRect();
	void updateLogoOpacity(int opacity);
	void updatePreviewCrop();
	void updatePreviewSize();
	void updateDimensionsNote();
	void toggleAdvanced();
	void updateAdvanced(bool enabled);
	void updateFramerateNote();
	void bindAnimationLoopSlider(KisSliderSpinBox *slider);
	void updateAnimationLoopText(KisSliderSpinBox *slider, int value);

	QSize getOutputSize() const;
	QRect getLogoRect();
	const QImage &getLogoImage();

	void showFfmpegSettings();
	void setFfmpegPath(const QString &ffmpegPath);
	void pickBackdropColor();
	void pickFlashColor();
	void pickColor(
		const QString &objectName, const QString &title,
		color_widgets::ColorPreview *preview, const QColor &resetColor);
	void confirmReset();
	QString choosePath(int format);

	void showSettingsPage();
	void showProgressPage();
	void showFinishPage();
	void updateExportButton();

	void updateProgressLabel(const QString &message);
	void setProgress(int percent);
	void handleSaveComplete(qint64 msecs);
	void handleSaveCancelled();
	void handleSaveFailed(const QString &errorMessage);

	drawdance::ViewModeBuffer m_vmb;
	drawdance::CanvasState m_canvasState;
	DP_ViewModeFilter m_vmf;
	QRect m_crop;
	QString m_inputPath;
	QScrollArea *m_scroll;
	widgets::TimelapsePreview *m_timelapsePreview;
	QWidget *m_settingsPage;
	QWidget *m_progressPage;
	QWidget *m_finishPage;
	QComboBox *m_formatCombo;
	utils::FormNote *m_ffmpegNote = nullptr;
	QTimeEdit *m_durationEdit;
	QSpinBox *m_widthSpinner;
	QSpinBox *m_heightSpinner;
	QCheckBox *m_keepAspectCheckBox;
	QCheckBox *m_cropCheckBox;
	QCheckBox *m_animationResultCheckBox = nullptr;
	QCheckBox *m_animationFlipbookCheckBox = nullptr;
	QButtonGroup *m_logoLocationGroup;
	utils::FormNote *m_dimensionsNote;
	QPushButton *m_advancedButton;
	QWidget *m_advancedWidget;
	QCheckBox *m_ffmpegCheckBox = nullptr;
	QComboBox *m_interpolationCombo;
	QCheckBox *m_ownCheckBox;
	color_widgets::ColorPreview *m_backdropPreview;
	KisDoubleSliderSpinBox *m_logoScaleSlider;
	KisDoubleSliderSpinBox *m_logoOffsetSlider;
	KisSliderSpinBox *m_logoOpacitySlider;
	KisDoubleSliderSpinBox *m_lingerBeforeSlider;
	KisSliderSpinBox *m_lingerAnimationBeforeSlider = nullptr;
	color_widgets::ColorPreview *m_flashPreview;
	KisDoubleSliderSpinBox *m_flashSlider;
	KisDoubleSliderSpinBox *m_lingerAfterSlider;
	KisSliderSpinBox *m_lingerAnimationAfterSlider = nullptr;
	KisDoubleSliderSpinBox *m_maxDeltaSlider;
	KisSliderSpinBox *m_maxQueueEntriesSlider;
	KisDoubleSliderSpinBox *m_framerateSlider;
	utils::FormNote *m_framerateNote;
	QPushButton *m_ffmpegButton = nullptr;
	QLabel *m_progressLabel;
	QProgressBar *m_progressBar;
	QLabel *m_finishLabel;
	QDialogButtonBox *m_buttons;
	QString m_ffmpegPath;
	TimelapseSaverRunnable *m_saver = nullptr;
	QImage m_logoImage;
	QSize m_fullResolution;
	QSize m_cropResolution;
	qreal m_aspectRatio = 0.0;
	int m_frameRangeFirst = 0;
	int m_frameRangeLast = 0;
	int m_flipbookFrameRangeFirst = 0;
	int m_flipbookFrameRangeLast = 0;
	double m_animationFramerate = 24.0;
	double m_flipbookFramerate = 24.0;
	bool m_inFrameView;
	bool m_cancelling = false;
};

}

#endif
