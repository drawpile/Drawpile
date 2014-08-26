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

#include "viewstatus.h"

#include <QLabel>
#include <QSlider>
#include <QHBoxLayout>

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

	_zoomSlider = new QSlider(Qt::Horizontal, this);
	_zoomSlider->setMaximumWidth(120);
	_zoomSlider->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum));
	_zoomSlider->setMinimum(50);
	_zoomSlider->setMaximum(1600);
	_zoomSlider->setPageStep(50);
	_zoomSlider->setValue(100);
	connect(_zoomSlider, &QSlider::valueChanged, [this](int val) { emit zoomChanged(val); });

	_zoom = new QLabel("100%", this);
	_zoom->setFixedWidth(_zoom->fontMetrics().width("9999.9%"));

	layout->addWidget(zlbl);
	layout->addWidget(_zoomSlider);
	layout->addWidget(_zoom);

	// Rotation angle
	layout->addSpacing(10);
	QLabel *rlbl = new QLabel(tr("Angle:"), this);
	_angle = new QLabel(QString::fromUtf8("0°"));
	_angle->setFixedWidth(_angle->fontMetrics().width("9999.9"));

	_angleSlider = new QSlider(Qt::Horizontal, this);
	_angleSlider->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum));
	_angleSlider->setMinimum(-360);
	_angleSlider->setMaximum(360);
	_angleSlider->setPageStep(45);
	connect(_angleSlider, &QSlider::valueChanged, [this](int val) { emit angleChanged(val); });

	layout->addWidget(rlbl);
	layout->addWidget(_angleSlider);
	layout->addWidget(_angle);
}

void ViewStatus::setTransformation(qreal zoom, qreal angle)
{
	_zoomSlider->setValue(zoom);
	_angleSlider->setValue(angle);
	_zoom->setText(QString::number(zoom, 'f', 0) + "%");
	_angle->setText(QString::number(angle, 'f', 1) + QChar(0xb0));
}

}

