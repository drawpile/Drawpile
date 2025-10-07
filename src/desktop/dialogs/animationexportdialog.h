// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_ANIMATIONEXPORTDIALOG
#define DESKTOP_DIALOGS_ANIMATIONEXPORTDIALOG
#include <QDialog>
#include <QHash>

class KisSliderSpinBox;
class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace canvas {
class CanvasModel;
}

namespace dialogs {

class AnimationExportDialog final : public QDialog {
	Q_OBJECT
public:
	explicit AnimationExportDialog(
		int loops, int scalePercent, bool scaleSmooth,
		QWidget *parent = nullptr);

	void setCanvas(canvas::CanvasModel *canvas);

	void setFlipbookState(
		int start, int end, double speedPercent, const QRectF &crop,
		bool apply);

	static QSize getScaledSizeFor(int scalePercent, const QRect &rect);

public slots:
#ifndef __EMSCRIPTEN__
	void accept() override;
#endif

signals:
	void exportRequested(
#ifndef __EMSCRIPTEN__
		const QString &path,
#endif
		int format, int loops, int start, int end, double framerate,
		const QRect &crop, int scalePercent, bool scaleSmooth);

private:
	void updateOutputUi();
	void updateScalingUi();
#ifndef __EMSCRIPTEN__
	QString choosePath();
#endif

	void updateStartRange(int end);
	void updateEndRange(int start);
	void updateX1Range(int x2);
	void updateX2Range(int x1);
	void updateY1Range(int y2);
	void updateY2Range(int y1);
	void resetInputs();
	void setInputsFromFlipbook();

	void setCanvasSize(const QSize &size);
	void setCanvasFrameCount(int frameCount);
	void setCanvasFramerate(double framerate);

	void requestExport();

	QRect getCropRect() const;

#ifndef __EMSCRIPTEN__
	QString m_path;
#endif
	QComboBox *m_formatCombo;
	QLabel *m_loopsLabel;
	QSpinBox *m_scaleSpinner;
	QCheckBox *m_scaleSmoothBox;
	QLabel *m_scaleLabel;
	QSpinBox *m_loopsSpinner;
	QSpinBox *m_startSpinner;
	QSpinBox *m_endSpinner;
	QDoubleSpinBox *m_framerateSpinner;
	QSpinBox *m_x1Spinner;
	QSpinBox *m_x2Spinner;
	QSpinBox *m_y1Spinner;
	QSpinBox *m_y2Spinner;
	QPushButton *m_inputResetButton;
	QPushButton *m_inputToFlipbookButton;
	QDialogButtonBox *m_buttons;
	int m_canvasWidth = -1;
	int m_canvasHeight = -1;
	int m_canvasFrameCount = -1;
	double m_canvasFramerate = -1.0;
	int m_flipbookStart = -1;
	int m_flipbookEnd = -1;
	double m_flipbookFramerate = -1.0;
	QRect m_flipbookCrop;
};

}

#endif
