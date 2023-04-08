// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/toolwidgets/zoomsettings.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>

namespace tools {

ZoomSettings::ZoomSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
{
}

QWidget *ZoomSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	auto layout = new QVBoxLayout(widget);

	auto *resetButton = new QPushButton(tr("Normal Size"), widget);
	auto *fitButton = new QPushButton(tr("Fit To Window"), widget);

	connect(resetButton, &QPushButton::clicked, this, &ZoomSettings::resetZoom);
	connect(fitButton, &QPushButton::clicked, this, &ZoomSettings::fitToWindow);

	layout->addWidget(resetButton);
	layout->addWidget(fitButton);
	layout->addStretch(1);

	return widget;
}

}

