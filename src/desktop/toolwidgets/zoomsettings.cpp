/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

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

