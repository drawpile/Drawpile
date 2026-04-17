// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/inputsettingsdialog.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/curvewidget.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QTabWidget>
#include <QVBoxLayout>
#include <libclient/config/config.h>

namespace dialogs {

InputSettingsDialog::InputSettingsDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Input Settings"));
	resize(400, 500);

	config::Config *cfg = dpAppConfig();
	QVBoxLayout *dlgLayout = new QVBoxLayout(this);

	m_tabs = new QTabWidget;
	dlgLayout->addWidget(m_tabs, 1);

	m_stabilizerPage = new QWidget;
	m_tabs->addTab(m_stabilizerPage, tr("Stabilization"));

	QVBoxLayout *stabilizerLayout = new QVBoxLayout(m_stabilizerPage);

	KisSliderSpinBox *stabilizerSampleCount = new KisSliderSpinBox;
	stabilizerSampleCount->setPrefix(tr("Stabilizer: "));

	m_stabilizerFinishStrokesBox = new QCheckBox(tr("Finish strokes"));
	stabilizerLayout->addWidget(m_stabilizerFinishStrokesBox);
	connect(
		m_stabilizerFinishStrokesBox, &QCheckBox::clicked, this,
		&InputSettingsDialog::stabilizerFinishStrokesChanged);

	QCheckBox *stabilizerVelocityEnabledBox =
		new QCheckBox(tr("Adjust with velocity"));
	stabilizerLayout->addWidget(stabilizerVelocityEnabledBox);
	CFG_BIND_CHECKBOX(
		cfg, StabilizerVelocityEnabled, stabilizerVelocityEnabledBox);

	utils::addFormSpacer(stabilizerLayout);

	m_stabilizerVelocityAdjustmentSlider = new KisSliderSpinBox;
	m_stabilizerVelocityAdjustmentSlider->setRange(1, 100);
	m_stabilizerVelocityAdjustmentSlider->setPrefix(tr("Maximum adjustment: "));
	m_stabilizerVelocityAdjustmentSlider->setSuffix(
		QCoreApplication::translate("Units", "%"));
	utils::setWidgetRetainSizeWhenHidden(
		m_stabilizerVelocityAdjustmentSlider, true);
	stabilizerLayout->addWidget(m_stabilizerVelocityAdjustmentSlider);
	CFG_BIND_SLIDERSPINBOX(
		cfg, StabilizerVelocityAdjustment,
		m_stabilizerVelocityAdjustmentSlider);

	m_stabilizerVelocityMaxSlider = new KisSliderSpinBox;
	m_stabilizerVelocityMaxSlider->setRange(1, 1000);
	m_stabilizerVelocityMaxSlider->setPrefix(tr("Maximum velocity: "));
	utils::setWidgetRetainSizeWhenHidden(m_stabilizerVelocityMaxSlider, true);
	stabilizerLayout->addWidget(m_stabilizerVelocityMaxSlider);
	CFG_BIND_SLIDERSPINBOX(
		cfg, StabilizerVelocityMax, m_stabilizerVelocityMaxSlider);

	utils::addFormSpacer(stabilizerLayout);

	m_stabilizerVelocityCurveWidget = new widgets::CurveWidget;
	m_stabilizerVelocityCurveWidget->setAxisTitleLabels(
		tr("Velocity"), tr("Adjustment"));
	m_stabilizerVelocityCurveWidget->setCurveSize(200, 200);
	m_stabilizerVelocityCurveWidget->setAxisValueLabelXMin(QStringLiteral("0"));
	// The leading spaces are so that this label is longer than the maximum
	// label. That way changing the maximum lable doesn't cause the widget to
	// jitter left and right depending on how many digits it shows.
	m_stabilizerVelocityCurveWidget->setAxisValueLabelYMin(
		QStringLiteral("    0%"));
	utils::setWidgetRetainSizeWhenHidden(m_stabilizerVelocityCurveWidget, true);
	stabilizerLayout->addWidget(m_stabilizerVelocityCurveWidget);
	CFG_BIND_CURVE_WIDGET(
		cfg, StabilizerVelocityCurve, m_stabilizerVelocityCurveWidget);

	CFG_BIND_SET(
		cfg, StabilizerVelocityEnabled, this,
		InputSettingsDialog::setStabilizerVelocityEnabled);
	CFG_BIND_SET(
		cfg, StabilizerVelocityAdjustment, this,
		InputSettingsDialog::setStabilizerVelocityAdjustment);
	CFG_BIND_SET(
		cfg, StabilizerVelocityMax, this,
		InputSettingsDialog::setStabilizerVelocityMax);

	stabilizerLayout->addStretch();

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
	dlgLayout->addWidget(buttons);
	connect(
		buttons, &QDialogButtonBox::accepted, this,
		&InputSettingsDialog::accept);
	connect(
		buttons, &QDialogButtonBox::rejected, this,
		&InputSettingsDialog::reject);
}

void InputSettingsDialog::showStabilizerPage()
{
	m_tabs->setCurrentWidget(m_stabilizerPage);
}

void InputSettingsDialog::setStabilizerFinishStrokes(
	bool stabilizerFinishStrokes)
{
	m_stabilizerFinishStrokesBox->setChecked(stabilizerFinishStrokes);
}

void InputSettingsDialog::setStabilizerVelocityAdjustment(
	int stabilizerVelocityAdjustment)
{
	m_stabilizerVelocityCurveWidget->setAxisValueLabelYMax(
		QStringLiteral("%1%").arg(stabilizerVelocityAdjustment));
}

void InputSettingsDialog::setStabilizerVelocityMax(int stabilizerVelocityMax)
{
	m_stabilizerVelocityCurveWidget->setAxisValueLabelXMax(
		QString::number(stabilizerVelocityMax));
}

void InputSettingsDialog::setStabilizerVelocityEnabled(
	bool stabilizerVelocityEnabled)
{
	utils::ScopedUpdateDisabler disabler(this);
	m_stabilizerVelocityAdjustmentSlider->setEnabled(stabilizerVelocityEnabled);
	m_stabilizerVelocityAdjustmentSlider->setVisible(stabilizerVelocityEnabled);
	m_stabilizerVelocityMaxSlider->setEnabled(stabilizerVelocityEnabled);
	m_stabilizerVelocityMaxSlider->setVisible(stabilizerVelocityEnabled);
	m_stabilizerVelocityCurveWidget->setEnabled(stabilizerVelocityEnabled);
	m_stabilizerVelocityCurveWidget->setVisible(stabilizerVelocityEnabled);
}

}
