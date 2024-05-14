// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/toolwidgets/toolsettings.h"
#include "libclient/tools/toolproperties.h"
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

}
