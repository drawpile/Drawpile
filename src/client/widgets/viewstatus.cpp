/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <QLabel>
#include <QHBoxLayout>
#include "viewstatus.h"

namespace widgets {

ViewStatus::ViewStatus(QWidget *parent)
{
	setMinimumHeight(16+2);
	QHBoxLayout *layout = new QHBoxLayout(this);

	layout->setMargin(1);
	layout->setSpacing(4);

	// Zoom level
	QLabel *zlbl = new QLabel(tr("Zoom:"), this);
	layout->addWidget(zlbl);
	zoom_ = new QLabel("100%", this);
	layout->addWidget(zoom_);

	// Rotation angle
	QLabel *rlbl = new QLabel(this);
	rlbl->setPixmap(QPixmap(":/icons/angle.png"));
	layout->addWidget(rlbl);
	angle_ = new QLabel(QString('0') + QChar(0xb0));
	layout->addWidget(angle_);
}

void ViewStatus::setTransformation(qreal zoom, qreal angle)
{
	zoom_->setText(QString::number(zoom, 'f', 0) + "%");
	angle_->setText(QString::number(angle, 'f', 1) + QChar(0xb0));
}

}

