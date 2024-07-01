// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/toolwidgets/gradientsettings.h"
#include <QWidget>

namespace tools {

GradientSettings::GradientSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
{
}

QWidget *GradientSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	// TODO
	return widget;
}

}
