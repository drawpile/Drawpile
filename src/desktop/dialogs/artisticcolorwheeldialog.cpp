// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/artisticcolorwheeldialog.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/artisticcolorwheel.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QVBoxLayout>

namespace dialogs {

ArtisticColorWheelDialog::ArtisticColorWheelDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Color Circle Settings"));
	setWindowModality(Qt::WindowModal);
	resize(350, 500);

	QVBoxLayout *layout = new QVBoxLayout(this);
	const desktop::settings::Settings &settings = dpApp().settings();

	bool hueLimit = settings.colorCircleHueLimit();
	m_continuousHueBox = new QCheckBox;
	m_continuousHueBox->setChecked(!hueLimit);
	layout->addWidget(m_continuousHueBox);
	connect(
		m_continuousHueBox, COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(QCheckBox),
		this, &ArtisticColorWheelDialog::updateContinuousHue);

	int hueCount = settings.colorCircleHueCount();
	m_hueStepsSlider = new KisSliderSpinBox;
	m_hueStepsSlider->setRange(
		widgets::ArtisticColorWheel::HUE_COUNT_MIN,
		widgets::ArtisticColorWheel::HUE_COUNT_MAX);
	m_hueStepsSlider->setValue(hueCount);
	m_hueStepsSlider->setEnabled(hueLimit);
	layout->addWidget(m_hueStepsSlider);
	connect(
		m_hueStepsSlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &ArtisticColorWheelDialog::updateHueSteps);

	int hueAngle = settings.colorCircleHueAngle();
	m_hueAngleSlider = new KisSliderSpinBox;
	m_hueAngleSlider->setRange(0, 180);
	m_hueAngleSlider->setValue(hueAngle);
	m_hueAngleSlider->setEnabled(hueLimit);
	layout->addWidget(m_hueAngleSlider);
	connect(
		m_hueAngleSlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &ArtisticColorWheelDialog::updateHueAngle);

	bool saturationLimit = settings.colorCircleSaturationLimit();
	m_continuousSaturationBox = new QCheckBox;
	m_continuousSaturationBox->setChecked(!saturationLimit);
	layout->addWidget(m_continuousSaturationBox);
	connect(
		m_continuousSaturationBox,
		COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(QCheckBox), this,
		&ArtisticColorWheelDialog::updateContinuousSaturation);

	int saturationCount = settings.colorCircleSaturationCount();
	m_saturationStepsSlider = new KisSliderSpinBox;
	m_saturationStepsSlider->setRange(
		widgets::ArtisticColorWheel::SATURATION_COUNT_MIN,
		widgets::ArtisticColorWheel::SATURATION_COUNT_MAX);
	m_saturationStepsSlider->setValue(saturationCount);
	m_saturationStepsSlider->setEnabled(saturationLimit);
	layout->addWidget(m_saturationStepsSlider);
	connect(
		m_saturationStepsSlider,
		QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&ArtisticColorWheelDialog::updateSaturationSteps);

	bool valueLimit = settings.colorCircleValueLimit();
	m_continuousValueBox = new QCheckBox;
	m_continuousValueBox->setChecked(!valueLimit);
	layout->addWidget(m_continuousValueBox);
	connect(
		m_continuousValueBox, COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(QCheckBox),
		this, &ArtisticColorWheelDialog::updateContinuousValue);

	int valueCount = settings.colorCircleValueCount();
	m_valueStepsSlider = new KisSliderSpinBox;
	m_valueStepsSlider->setRange(
		widgets::ArtisticColorWheel::VALUE_COUNT_MIN,
		widgets::ArtisticColorWheel::VALUE_COUNT_MAX);
	m_valueStepsSlider->setValue(valueCount);
	m_valueStepsSlider->setEnabled(valueLimit);
	layout->addWidget(m_valueStepsSlider);
	connect(
		m_valueStepsSlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &ArtisticColorWheelDialog::updateValueSteps);

	m_continuousHueBox->setText(tr("Continuous hue"));
	m_hueStepsSlider->setPrefix(tr("Hue steps: "));
	m_hueAngleSlider->setPrefix(tr("Hue angle: "));
	//: Degree symbol. Unless your language uses a different one, keep as-is.
	m_hueAngleSlider->setSuffix(tr("°"));
	ColorSpace colorSpace = settings.colorWheelSpace();
	if(colorSpace == ColorSpace::ColorLCH) {
		m_continuousSaturationBox->setText(tr("Continuous chroma"));
		m_saturationStepsSlider->setPrefix(tr("Chroma steps: "));
		m_continuousValueBox->setText(tr("Continuous luminance"));
		m_saturationStepsSlider->setPrefix(tr("Luminance steps: "));
	} else {
		m_continuousSaturationBox->setText(tr("Continuous saturation"));
		m_saturationStepsSlider->setPrefix(tr("Saturation steps: "));
		if(colorSpace == ColorSpace::ColorHSL) {
			m_continuousValueBox->setText(tr("Continuous lightness"));
			m_valueStepsSlider->setPrefix(tr("Lightness steps: "));
		} else {
			m_continuousValueBox->setText(tr("Continuous value"));
			m_valueStepsSlider->setPrefix(tr("Value steps: "));
		}
	}

	m_wheel = new widgets::ArtisticColorWheel;
	m_wheel->setHueLimit(hueLimit);
	m_wheel->setHueCount(hueCount);
	m_wheel->setHueAngle(hueAngle);
	m_wheel->setSaturationLimit(saturationLimit);
	m_wheel->setSaturationCount(saturationCount);
	m_wheel->setValueLimit(valueLimit);
	m_wheel->setValueCount(valueCount);
	m_wheel->setColorSpace(colorSpace);
	m_wheel->setColor(Qt::red);
	m_wheel->setMinimumSize(64, 64);
	layout->addWidget(m_wheel, 1);

	QDialogButtonBox *buttons = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Apply |
		QDialogButtonBox::Cancel);
	layout->addWidget(buttons);
	connect(
		buttons, &QDialogButtonBox::accepted, this,
		&ArtisticColorWheelDialog::accept);
	connect(
		buttons, &QDialogButtonBox::rejected, this,
		&ArtisticColorWheelDialog::reject);
	connect(
		buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
		&ArtisticColorWheelDialog::saveSettings);

	connect(
		this, &ArtisticColorWheelDialog::accepted, this,
		&ArtisticColorWheelDialog::saveSettings);
}

void ArtisticColorWheelDialog::updateContinuousHue(compat::CheckBoxState state)
{
	bool limit = state == Qt::Unchecked;
	m_hueStepsSlider->setEnabled(limit);
	m_hueAngleSlider->setEnabled(limit);
	m_wheel->setHueLimit(limit);
}

void ArtisticColorWheelDialog::updateHueSteps(int steps)
{
	m_wheel->setHueCount(steps);
}

void ArtisticColorWheelDialog::updateHueAngle(int angle)
{
	m_wheel->setHueAngle(angle);
}

void ArtisticColorWheelDialog::updateContinuousSaturation(
	compat::CheckBoxState state)
{
	bool limit = state == Qt::Unchecked;
	m_saturationStepsSlider->setEnabled(limit);
	m_wheel->setSaturationLimit(limit);
}

void ArtisticColorWheelDialog::updateSaturationSteps(int steps)
{
	m_wheel->setSaturationCount(steps);
}

void ArtisticColorWheelDialog::updateContinuousValue(
	compat::CheckBoxState state)
{
	bool limit = state == Qt::Unchecked;
	m_valueStepsSlider->setEnabled(limit);
	m_wheel->setValueLimit(limit);
}

void ArtisticColorWheelDialog::updateValueSteps(int steps)
{
	m_wheel->setValueCount(steps);
}

void ArtisticColorWheelDialog::saveSettings()
{
	desktop::settings::Settings &settings = dpApp().settings();
	settings.setColorCircleHueLimit(!m_continuousHueBox->isChecked());
	settings.setColorCircleHueCount(m_hueStepsSlider->value());
	settings.setColorCircleHueAngle(m_hueAngleSlider->value());
	settings.setColorCircleSaturationLimit(
		!m_continuousSaturationBox->isChecked());
	settings.setColorCircleSaturationCount(m_saturationStepsSlider->value());
	settings.setColorCircleValueLimit(!m_continuousValueBox->isChecked());
	settings.setColorCircleValueCount(m_valueStepsSlider->value());
}

}
