// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/toolwidgets/pansettings.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include <QButtonGroup>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace tools {

PanSettings::PanSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
{
}

QWidget *PanSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	// The pan tool has no settings.
	return widget;
}

}
