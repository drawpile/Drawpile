// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_ARTISTICCOLORWHEELDIALOG
#define DESKTOP_DIALOGS_ARTISTICCOLORWHEELDIALOG
#include "desktop/utils/qtguicompat.h"
#include <QDialog>
#include <QtColorWidgets/ColorWheel>

class KisSliderSpinBox;
class QCheckBox;
class QColor;
class QComboBox;

namespace widgets {
class ArtisticColorWheel;
}

namespace dialogs {

class ArtisticColorWheelDialog : public QDialog {
	Q_OBJECT
public:
	using ColorSpace = color_widgets::ColorWheel::ColorSpaceEnum;

	explicit ArtisticColorWheelDialog(QWidget *parent = nullptr);

private:
	void loadGamutMasks();
	void updateContinuousHue(compat::CheckBoxState state);
	void updateHueSteps(int steps);
	void updateHueAngle(int angle);
	void updateContinuousSaturation(compat::CheckBoxState state);
	void updateSaturationSteps(int steps);
	void updateContinuousValue(compat::CheckBoxState state);
	void updateValueSteps(int steps);
	void updateGamutMask(int index);
	void updateGamutMaskAngle(int angle);
	void updateGamutMaskOpacity(int opacity);
	void saveSettings();

	QCheckBox *m_continuousHueBox;
	KisSliderSpinBox *m_hueStepsSlider;
	KisSliderSpinBox *m_hueAngleSlider;
	QCheckBox *m_continuousSaturationBox;
	KisSliderSpinBox *m_saturationStepsSlider;
	QCheckBox *m_continuousValueBox;
	KisSliderSpinBox *m_valueStepsSlider;
	QComboBox *m_gamutMaskCombo;
	KisSliderSpinBox *m_gamutMaskAngleSlider;
	KisSliderSpinBox *m_gamutMaskOpacitySlider;
	widgets::ArtisticColorWheel *m_wheel;
};

}

#endif
