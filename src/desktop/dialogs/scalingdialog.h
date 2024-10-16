// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SCALINGDIALOG_H
#define DESKTOP_DIALOGS_SCALINGDIALOG_H
#include <QDialog>

class QButtonGroup;
class QPushButton;
class QSlider;
class QSpinBox;

namespace dialogs {

class ScalingDialog : public QDialog {
	Q_OBJECT
public:
	explicit ScalingDialog(QWidget *parent = nullptr);

	bool scalingOverride(qreal &outFactor) const;
	void setScalingOverride(bool scalingOverride, qreal factor);

private:
	void scalingSliderChanged(int value);
	void updateUi();

	QButtonGroup *m_group;
	QSlider *m_scalingSlider;
	QSpinBox *m_scalingSpinner;
	QPushButton *m_okButton;
	bool m_hadOverride;
	int m_currentScaleFactor;
};

}

#endif
