// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/toolwidgets/toolsettings.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libclient/tools/toolproperties.h"
#include <QAbstractButton>
#include <QButtonGroup>
#include <functional>

namespace tools {

QWidget *ToolSettings::createUi(QWidget *parent)
{
	Q_ASSERT(!m_widget);
	m_widget = createUiWidget(parent);
	return m_widget;
}

void ToolSettings::pushSettings() {}

ToolProperties ToolSettings::saveToolSettings()
{
	return ToolProperties();
}

void ToolSettings::restoreToolSettings(const ToolProperties &) {}

static int
step(int min, int max, int current, bool increase, std::function<int(int)> inc)
{
	int size = min;
	while(size <= current) {
		int next = inc(size);
		if(increase) {
			if(next > current) {
				return qMin(next, max);
			}
		} else if(size < current && next >= current) {
			return qMax(size, min);
		}
		size = next;
	}
	return increase ? max : min;
}

int ToolSettings::stepLogarithmic(
	int min, int max, int current, bool increase, double stepSize)
{
	return step(min, max, current, increase, [&](int size) {
		return size + qMax(1, qCeil(size / stepSize));
	});
}

int ToolSettings::stepLinear(
	int min, int max, int current, bool increase, int stepSize)
{
	return step(min, max, current, increase, [&](int size) {
		return size + stepSize;
	});
}

void ToolSettings::checkGroupButton(QButtonGroup *group, int id)
{
	QAbstractButton *button = group->button(id);
	if(button) {
		button->setChecked(true);
	}
}

void ToolSettings::quickAdjustOn(
	KisSliderSpinBox *slider, qreal adjustment, bool wheel, qreal &quickAdjustN)
{
	if(slider && slider->isEnabled()) {
		if(wheel) {
			int i;
			if(adjustment < 0.0) {
				i = qMin(-1, qRound(adjustment));
			} else if(adjustment > 0.0) {
				i = qMax(1, qRound(adjustment));
			} else {
				return;
			}
			quickAdjustN = 0.0;
			adjustSlider(slider, slider->value() + i);
		} else {
			quickAdjustN += adjustment;
			qreal i;
			qreal f = modf(quickAdjustN, &i);
			int delta = int(i);
			if(delta != 0) {
				quickAdjustN = f;
				adjustSlider(slider, slider->value() + delta);
			}
		}
	}
}

void ToolSettings::adjustSlider(KisSliderSpinBox *slider, int value)
{
	if(slider->isSoftRangeActive()) {
		slider->setValue(
			qBound(slider->softMinimum(), value, slider->softMaximum()));
	} else {
		slider->setValue(value);
	}
}

}
