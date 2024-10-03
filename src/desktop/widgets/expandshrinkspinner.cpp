// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/flood_fill.h>
}
#include "desktop/widgets/expandshrinkspinner.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include <QAction>
#include <QActionGroup>
#include <QHBoxLayout>
#include <QMenu>
#include <QSpinBox>
#include <QToolButton>
#include <functional>

namespace widgets {

ExpandShrinkSpinner::ExpandShrinkSpinner(bool slider, QWidget *parent)
	: QWidget(parent)
	, m_shrink(false)
	, m_kernel(int(DP_FLOOD_FILL_KERNEL_ROUND))
{
	if(slider) {
		m_spinner = new KisSliderSpinBox;
	} else {
		m_spinner = new QSpinBox;
	}
	m_spinner->setSuffix(tr("px"));
	connect(
		m_spinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&ExpandShrinkSpinner::spinnerValueChanged);

	m_directionButton = new QToolButton;
	m_kernelButton = new QToolButton;
	m_directionButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	m_kernelButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	m_directionButton->setPopupMode(QToolButton::InstantPopup);
	m_kernelButton->setPopupMode(QToolButton::InstantPopup);

	QMenu *directionMenu = new QMenu(m_directionButton);
	QMenu *kernelMenu = new QMenu(m_kernelButton);
	m_directionButton->setMenu(directionMenu);
	m_kernelButton->setMenu(kernelMenu);

	m_expandAction =
		directionMenu->addAction(QIcon::fromTheme("zoom-in"), tr("Expand"));
	m_shrinkAction =
		directionMenu->addAction(QIcon::fromTheme("zoom-out"), tr("Shrink"));
	m_roundAction = kernelMenu->addAction(
		QIcon::fromTheme("drawpile_pixelround"), tr("Round expansion shape"));
	m_squareAction = kernelMenu->addAction(
		QIcon::fromTheme("drawpile_square"), tr("Square kernel shape"));

	connect(
		m_expandAction, &QAction::triggered, this,
		std::bind(&ExpandShrinkSpinner::setShrink, this, false));
	connect(
		m_shrinkAction, &QAction::triggered, this,
		std::bind(&ExpandShrinkSpinner::setShrink, this, true));
	connect(
		m_roundAction, &QAction::triggered, this,
		std::bind(
			&ExpandShrinkSpinner::setKernel, this,
			int(DP_FLOOD_FILL_KERNEL_ROUND)));
	connect(
		m_squareAction, &QAction::triggered, this,
		std::bind(
			&ExpandShrinkSpinner::setKernel, this,
			int(DP_FLOOD_FILL_KERNEL_SQUARE)));

	setContentsMargins(0, 0, 0, 0);
	QHBoxLayout *layout = new QHBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(m_spinner, 1);
	layout->addSpacing(3);
	layout->addWidget(m_directionButton);
	layout->addSpacing(3);
	layout->addWidget(m_kernelButton);
	setLayout(layout);

	updateDirectionLabels();
	updateKernelLabel();
}

void ExpandShrinkSpinner::setBlockUpdateSignalOnDrag(bool block)
{
	KisSliderSpinBox *slider = qobject_cast<KisSliderSpinBox *>(m_spinner);
	if(slider) {
		slider->setBlockUpdateSignalOnDrag(block);
	}
}

void ExpandShrinkSpinner::setSpinnerRange(int min, int max)
{
	m_spinner->setRange(min, max);
}

void ExpandShrinkSpinner::setSpinnerValue(int value)
{
	m_spinner->setValue(value);
}

int ExpandShrinkSpinner::spinnerValue() const
{
	return m_spinner->value();
}

int ExpandShrinkSpinner::effectiveValue() const
{
    int value = spinnerValue();
    return m_shrink ? -value : value;
}

void ExpandShrinkSpinner::setShrink(bool shrink)
{
	if(shrink != m_shrink) {
		m_shrink = shrink;
		updateDirectionLabels();
		emit shrinkChanged(shrink);
	}
}

void ExpandShrinkSpinner::setKernel(int kernel)
{
	switch(kernel) {
	case int(DP_FLOOD_FILL_KERNEL_ROUND):
	case int(DP_FLOOD_FILL_KERNEL_SQUARE):
		if(kernel != m_kernel) {
			m_kernel = kernel;
			updateKernelLabel();
			kernelChanged(kernel);
		}
		break;
	default:
		qWarning("Unknown kernel %d", kernel);
		break;
	}
}

void ExpandShrinkSpinner::updateDirectionLabels()
{
	updateButtonLabelFromAction(
		m_directionButton, m_shrink ? m_shrinkAction : m_expandAction);
	m_spinner->setPrefix(m_shrink ? tr("Shrink: ") : tr("Expand: "));
}

void ExpandShrinkSpinner::updateKernelLabel()
{
	updateButtonLabelFromAction(
		m_kernelButton, m_kernel == int(DP_FLOOD_FILL_KERNEL_ROUND)
							? m_roundAction
							: m_squareAction);
}

void ExpandShrinkSpinner::updateButtonLabelFromAction(
	QToolButton *button, QAction *action)
{
	button->setIcon(action->icon());
	button->setToolTip(action->text());
	button->setStatusTip(action->text());
}
}
