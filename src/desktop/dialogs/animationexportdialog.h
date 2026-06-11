// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_ANIMATIONEXPORTDIALOG
#define DESKTOP_DIALOGS_ANIMATIONEXPORTDIALOG
#include <QDialog>
#include <QVector>

class KisDoubleSliderSpinBox;
class KisSliderSpinBox;
class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace canvas {
class CanvasModel;
}

namespace utils {
class FormNote;
}

namespace dialogs {

class AnimationExportDialog final : public QDialog {
	Q_OBJECT
public:
	explicit AnimationExportDialog(
		int loops, double scalePercent, bool scaleSmooth,
		QWidget *parent = nullptr);

	void setCanvas(canvas::CanvasModel *canvas);

	void setFlipbookState(
		int start, int end, double speedPercent, const QRectF &crop,
		bool apply);

	static QSize getScaledSizeFor(double scalePercent, const QRect &rect);

public slots:
#ifndef __EMSCRIPTEN__
	void accept() override;
#endif

signals:
	void exportRequested(
#ifndef __EMSCRIPTEN__
		const QString &path,
#endif
#ifdef DP_ANDROID_VIDEO_ENCODER
		bool useAndroidVideoEncoder,
		bool useHardware,
#else
		const QString &ffmpegPath,
#endif
		int format, int loops, const QVector<int> &frameIndexes,
		double framerate, const QRect &crop, double scalePercent,
		bool scaleSmooth);

private:
	void updateOutputUi();
	void updateScalingUi();
	void updateEncoderUi();
#ifndef DP_ANDROID_VIDEO_ENCODER
	void showFfmpegSettings();
	void setFfmpegPath(const QString &ffmpegPath);
	void updateFfmpegFormatIcons();
#endif
#ifndef __EMSCRIPTEN__
	QString choosePath();
#endif

	void updateX1Range(int x2);
	void updateX2Range(int x1);
	void updateY1Range(int y2);
	void updateY2Range(int y1);
	void resetInputs();
	void setInputsFromFlipbook();

	void setCanvasSize(const QSize &size);
	void setCanvasFrameRange(int frameRangeFirst, int frameRangeLast);
	void setCanvasFrameCount(int frameCount);
	void setCanvasFramerate(double framerate);

	void requestExport();

	QRect getCropRect() const;

	QVector<int> buildFrameIndexes() const;

#ifndef __EMSCRIPTEN__
	QString m_path;
#endif
	QComboBox *m_formatCombo;
	QLabel *m_loopsLabel;
	KisDoubleSliderSpinBox *m_scaleSpinner;
	QCheckBox *m_scaleSmoothBox;
	QLabel *m_scaleLabel;
	KisSliderSpinBox *m_loopsSpinner;
	KisSliderSpinBox *m_startSpinner;
	KisSliderSpinBox *m_endSpinner;
	KisDoubleSliderSpinBox *m_framerateSpinner;
	KisSliderSpinBox *m_x1Spinner;
	KisSliderSpinBox *m_x2Spinner;
	KisSliderSpinBox *m_y1Spinner;
	KisSliderSpinBox *m_y2Spinner;
	QPushButton *m_inputResetButton;
	QPushButton *m_inputToFlipbookButton;
	QDialogButtonBox *m_buttons;
	int m_canvasWidth = -1;
	int m_canvasHeight = -1;
	int m_canvasFrameCount = -1;
	int m_canvasFrameRangeFirst = -1;
	int m_canvasFrameRangeLast = -1;
	double m_canvasFramerate = -1.0;
	int m_flipbookStart = -1;
	int m_flipbookEnd = -1;
	double m_flipbookFramerate = -1.0;
	QRect m_flipbookCrop;
#ifdef DP_ANDROID_VIDEO_ENCODER
	QCheckBox *m_androidCheckBox;
	QCheckBox *m_androidHardwareCheckBox;
#else
	utils::FormNote *m_ffmpegNote = nullptr;
	QCheckBox *m_ffmpegCheckBox = nullptr;
	QPushButton *m_ffmpegButton = nullptr;
	QString m_ffmpegPath;
#endif
};

}

#endif
