// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/scalingdialog.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>
#include <QWindow>

namespace dialogs {

ScalingDialog::ScalingDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Scaling"));
	QVBoxLayout *layout = new QVBoxLayout(this);

	const desktop::settings::Settings &settings = dpApp().settings();
	m_hadOverride = settings.overrideScaleFactor();
	m_currentScaleFactor = qRound(settings.scaleFactor() * 100.0);

	m_group = new QButtonGroup(this);

	QRadioButton *systemRadio = new QRadioButton(
		tr("Scale user interface according to system settings"));
	m_group->addButton(systemRadio, 0);
	layout->addWidget(systemRadio);

	utils::addFormSpacer(layout);

	QRadioButton *overrideRadio =
		new QRadioButton(tr("Use custom user interface scaling"));
	m_group->addButton(overrideRadio, 1);
	layout->addWidget(overrideRadio);

	m_group->button(settings.overrideScaleFactor() ? 1 : 0)->setChecked(true);
	connect(
		m_group, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
		this, &ScalingDialog::updateUi);

	QHBoxLayout *sliderLayout = new QHBoxLayout;
	sliderLayout->setContentsMargins(0, 0, 0, 0);

	m_scalingSlider = new QSlider(Qt::Horizontal);
	m_scalingSlider->setRange(0, 12);
	m_scalingSlider->setSingleStep(1);
	m_scalingSlider->setPageStep(1);
	m_scalingSlider->setTickPosition(QSlider::TicksBelow);
	sliderLayout->addWidget(m_scalingSlider);
	connect(
		m_scalingSlider, QOverload<int>::of(&QSlider::valueChanged), this,
		&ScalingDialog::scalingSliderChanged);

	m_scalingSpinner = new QSpinBox;
	m_scalingSpinner->setSuffix(tr("%"));
	m_scalingSpinner->setRange(100, 400);
	m_scalingSpinner->setSingleStep(25);
	m_scalingSpinner->setValue(m_currentScaleFactor);
	sliderLayout->addWidget(m_scalingSpinner);
	connect(
		m_scalingSpinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&ScalingDialog::updateUi);

	layout->addLayout(sliderLayout);

	utils::addFormSpacer(layout);
	layout->addStretch();

	QDialogButtonBox *buttons =
		new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	m_okButton = buttons->button(QDialogButtonBox::Ok);
	layout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::accepted, this, &ScalingDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &ScalingDialog::reject);

	updateUi();
	resize(400, 0);
}

bool ScalingDialog::scalingOverride(qreal &outFactor) const
{
	outFactor = m_scalingSpinner->value() / 100.0;
	return m_group->checkedId() == 1;
}

void ScalingDialog::setScalingOverride(bool scalingOverride, qreal factor)
{
	m_group->button(scalingOverride ? 1 : 0)->setChecked(true);
	m_scalingSpinner->setValue(qRound(factor * 100.0));
	updateUi();
}

void ScalingDialog::scalingSliderChanged(int value)
{
	m_scalingSpinner->setValue(100 + 25 * value);
}

void ScalingDialog::updateUi()
{
	bool haveOverride = m_group->checkedId() == 1;
	m_scalingSlider->setEnabled(haveOverride);
	m_scalingSpinner->setEnabled(haveOverride);

	int sliderValue = qRound((qreal(m_scalingSpinner->value()) - 100.0) / 25.0);
	if(sliderValue != m_scalingSlider->value()) {
		QSignalBlocker blocker(m_scalingSlider);
		m_scalingSlider->setValue(sliderValue);
	}

	m_okButton->setEnabled(
		haveOverride ? !m_hadOverride ||
						   m_scalingSpinner->value() != m_currentScaleFactor
					 : m_hadOverride);
}

}
