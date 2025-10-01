// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/animationpropertiesdialog.h"
#include "desktop/utils/widgetutils.h"
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace dialogs {

AnimationPropertiesDialog::AnimationPropertiesDialog(
	double framerate, int frameRangeFirst, int frameRangeLast,
	bool compatibilityMode, QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Animation Properties"));
	utils::makeModal(this);
	resize(400, 300);

	QVBoxLayout *layout = new QVBoxLayout;
	setLayout(layout);

	QFormLayout *form = new QFormLayout;
	form->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(form);

	m_framerateBox = new QDoubleSpinBox;
	if(compatibilityMode) {
		m_framerateBox->setDecimals(0);
		m_framerateBox->setRange(1.0, 999);
	} else {
		m_framerateBox->setDecimals(2);
		m_framerateBox->setRange(0.01, 999.99);
	}
	m_framerateBox->setValue(framerate);

	QHBoxLayout *framerateLayout = new QHBoxLayout;
	framerateLayout->addWidget(m_framerateBox, 1);
	framerateLayout->addWidget(new QLabel(tr("FPS")));
	form->addRow(tr("Framerate:"), framerateLayout);

	m_frameRangeFirstBox = new QSpinBox;
	m_frameRangeFirstBox->setRange(1, frameRangeLast + 1);
	if(compatibilityMode) {
		m_frameRangeFirstBox->setValue(1);
		m_frameRangeFirstBox->setEnabled(false);
	} else {
		m_frameRangeFirstBox->setValue(frameRangeFirst + 1);
	}
	form->addRow(tr("First frame:"), m_frameRangeFirstBox);
	connect(
		m_frameRangeFirstBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&AnimationPropertiesDialog::updateFrameRangeFirst);

	m_frameRangeLastBox = new QSpinBox;
	m_frameRangeLastBox->setRange(frameRangeFirst + 1, 9999);
	m_frameRangeLastBox->setValue(frameRangeLast + 1);
	form->addRow(tr("Last frame:"), m_frameRangeLastBox);
	connect(
		m_frameRangeLastBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&AnimationPropertiesDialog::updateFrameRangeLast);

	layout->addStretch();

	QDialogButtonBox *buttons =
		new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	layout->addWidget(buttons);
	connect(
		buttons, &QDialogButtonBox::accepted, this,
		&AnimationPropertiesDialog::accept);
	connect(
		buttons, &QDialogButtonBox::rejected, this,
		&AnimationPropertiesDialog::reject);

	connect(
		this, &AnimationPropertiesDialog::accepted, this,
		&AnimationPropertiesDialog::emitPropertiesChanged);
}

void AnimationPropertiesDialog::updateFrameRangeFirst(int frameRangeFirst)
{
	m_frameRangeLastBox->setMinimum(frameRangeFirst);
}

void AnimationPropertiesDialog::updateFrameRangeLast(int frameRangeLast)
{
	m_frameRangeFirstBox->setMaximum(frameRangeLast);
}

void AnimationPropertiesDialog::emitPropertiesChanged()
{
	emit propertiesChanged(
		m_framerateBox->value(), m_frameRangeFirstBox->value() - 1,
		m_frameRangeLastBox->value() - 1);
}

}
