/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2014 Calle Laakkonen

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

#include <QLabel>
#include <QHBoxLayout>

#include "viewstatus.h"

namespace widgets {

ViewStatus::ViewStatus(QWidget *parent)
	: QWidget(parent)
{
	setMinimumHeight(16+2);
	QHBoxLayout *layout = new QHBoxLayout(this);

	layout->setMargin(1);
	layout->setSpacing(4);

	// Zoom level
	QLabel *zlbl = new QLabel(tr("Zoom:"), this);
	layout->addWidget(zlbl);
	_zoom = new QLabel("100%", this);
	layout->addWidget(_zoom);

	// Rotation angle
	layout->addSpacing(10);
	QLabel *rlbl = new QLabel(tr("Angle:"), this);
	layout->addWidget(rlbl);
	_angle = new QLabel(QString::fromUtf8("0°"));
	layout->addWidget(_angle);
}

void ViewStatus::setTransformation(qreal zoom, qreal angle)
{
	_zoom->setText(QString::number(zoom, 'f', 0) + "%");
	_angle->setText(QString::number(angle, 'f', 1) + QChar(0xb0));
}

}

