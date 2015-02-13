/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2015 Calle Laakkonen

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
#include "utils/icon.h"

#include <QLabel>
#include <QSlider>
#include <QHBoxLayout>
#include <QAction>
#include <QToolButton>

namespace widgets {

ViewStatus::ViewStatus(QWidget *parent)
	: QWidget(parent)
{
	setMinimumHeight(22);
	QHBoxLayout *layout = new QHBoxLayout(this);

	layout->setMargin(1);
	layout->setSpacing(0);

	// View flipping
	layout->addSpacing(10);
	_viewFlip = new QToolButton(this);
	_viewFlip->setAutoRaise(true);

	_viewMirror = new QToolButton(this);
	_viewMirror->setAutoRaise(true);

	layout->addWidget(_viewFlip);
	layout->addWidget(_viewMirror);

	// Rotation angle
	layout->addSpacing(10);
	_resetRotation = new QToolButton(this);
	_resetRotation->setAutoRaise(true);

	auto *rotateLeft = new QToolButton(this);
	rotateLeft->setAutoRaise(true);
	rotateLeft->setIcon(icon::fromTheme("object-rotate-left"));
	connect(rotateLeft, &QToolButton::clicked, this, &ViewStatus::rotateLeft);

	auto *rotateRight = new QToolButton(this);
	rotateRight->setAutoRaise(true);
	rotateRight->setIcon(icon::fromTheme("object-rotate-right"));
	connect(rotateRight, &QToolButton::clicked, this, &ViewStatus::rotateRight);

	_angle = new QLabel(QString::fromUtf8("0°"));
	_angle->setFixedWidth(_angle->fontMetrics().width("9999.9"));
	_angle->setContextMenuPolicy(Qt::ActionsContextMenu);

	_angleSlider = new QSlider(Qt::Horizontal, this);
	_angleSlider->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum));
	_angleSlider->setMinimum(-360);
	_angleSlider->setMaximum(360);
	_angleSlider->setPageStep(45);
	_angleSlider->setContextMenuPolicy(Qt::ActionsContextMenu);
	connect(_angleSlider, &QSlider::valueChanged, [this](int val) { emit angleChanged(val); });

	_angleSlider->setToolTip(tr("Drag the view while holding ctrl-space to rotate"));

	layout->addWidget(_resetRotation);
	layout->addWidget(rotateLeft);
	layout->addWidget(_angleSlider);
	layout->addWidget(rotateRight);
	layout->addWidget(_angle);

	addAngleShortcut(-180);
	addAngleShortcut(-135);
	addAngleShortcut(-90);
	addAngleShortcut(-45);
	addAngleShortcut(0);
	addAngleShortcut(45);
	addAngleShortcut(90);
	addAngleShortcut(135);
	addAngleShortcut(180);

	// Zoom level
	_zoomIn = new QToolButton(this);
	_zoomIn->setAutoRaise(true);
	_zoomOut = new QToolButton(this);
	_zoomOut->setAutoRaise(true);
	_zoomOriginal = new QToolButton(this);
	_zoomOriginal->setAutoRaise(true);


	_zoomSlider = new QSlider(Qt::Horizontal, this);
	_zoomSlider->setMaximumWidth(120);
	_zoomSlider->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum));
	_zoomSlider->setMinimum(50);
	_zoomSlider->setMaximum(1600);
	_zoomSlider->setPageStep(50);
	_zoomSlider->setValue(100);
	_zoomSlider->setContextMenuPolicy(Qt::ActionsContextMenu);
	connect(_zoomSlider, &QSlider::valueChanged, [this](int val) { emit zoomChanged(val); });

	_zoom = new QLabel("100%", this);
	_zoom->setFixedWidth(_zoom->fontMetrics().width("9999.9%"));
	_zoom->setContextMenuPolicy(Qt::ActionsContextMenu);

	layout->addWidget(_zoomOriginal);
	layout->addWidget(_zoomOut);
	layout->addWidget(_zoomSlider);
	layout->addWidget(_zoomIn);
	layout->addWidget(_zoom);

	addZoomShortcut(50);
	addZoomShortcut(100);
	addZoomShortcut(200);
	addZoomShortcut(400);
}

void ViewStatus::setZoomActions(QAction *zoomIn, QAction *zoomOut, QAction *zoomOriginal)
{
	_zoomIn->setDefaultAction(zoomIn);
	_zoomOut->setDefaultAction(zoomOut);
	_zoomOriginal->setDefaultAction(zoomOriginal);
}

void ViewStatus::setRotationActions(QAction *resetRotation)
{
	_resetRotation->setDefaultAction(resetRotation);
	// Currently there are no external actions for rotation buttons
}

void ViewStatus::setFlipActions(QAction *flip, QAction *mirror)
{
	_viewFlip->setDefaultAction(flip);
	_viewMirror->setDefaultAction(mirror);
}

void ViewStatus::addZoomShortcut(int zoomLevel)
{
	QAction *a = new QAction(QString("%1%").arg(zoomLevel), this);
	_zoom->addAction(a);
	_zoomSlider->addAction(a);
	connect(a, &QAction::triggered, [this, zoomLevel]() {
		emit zoomChanged(zoomLevel);
	});
}

void ViewStatus::addAngleShortcut(int angle)
{
	QAction *a = new QAction(QString("%1°").arg(angle), this);
	_angle->addAction(a);
	_angleSlider->addAction(a);
	connect(a, &QAction::triggered, [this, angle]() {
		emit angleChanged(angle);
	});
}

void ViewStatus::setTransformation(qreal zoom, qreal angle)
{
	_zoomSlider->setValue(zoom);
	_angleSlider->setValue(angle);
	_zoom->setText(QString::number(zoom, 'f', 0) + "%");
	_angle->setText(QString::number(angle, 'f', 1) + QChar(0xb0));
}

void ViewStatus::rotateLeft()
{
	int a = _angleSlider->value() - 10;
	if(a < _angleSlider->minimum())
		a = _angleSlider->maximum() - 10;
	_angleSlider->setValue(a);
}

void ViewStatus::rotateRight()
{
	int a = _angleSlider->value() + 10;
	if(a > _angleSlider->maximum())
		a = _angleSlider->minimum() + 10;
	_angleSlider->setValue(a);
}

}
