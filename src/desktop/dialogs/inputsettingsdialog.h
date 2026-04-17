// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_INPUTSETTINGSDIALOG_H
#define DESKTOP_DIALOGS_INPUTSETTINGSDIALOG_H
#include <QDialog>

class QCheckBox;
class KisSliderSpinBox;
class QTabWidget;

namespace widgets {
class CurveWidget;
}

namespace dialogs {

class InputSettingsDialog : public QDialog {
	Q_OBJECT
public:
	InputSettingsDialog(QWidget *parent = nullptr);

	void showStabilizerPage();

	void setStabilizerFinishStrokes(bool stabilizerFinishStrokes);

Q_SIGNALS:
	void stabilizerFinishStrokesChanged(bool stabilizerFinishStrokes);

private:
	void setStabilizerVelocityAdjustment(int stabilizerVelocityAdjustment);
	void setStabilizerVelocityMax(int stabilizerVelocityMax);
	void setStabilizerVelocityEnabled(bool stabilizerVelocityEnabled);

	QTabWidget *m_tabs;
	QWidget *m_stabilizerPage;
	QCheckBox *m_stabilizerFinishStrokesBox;
	KisSliderSpinBox *m_stabilizerVelocityAdjustmentSlider;
	KisSliderSpinBox *m_stabilizerVelocityMaxSlider;
	widgets::CurveWidget *m_stabilizerVelocityCurveWidget;
};

}

#endif
