/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2009 Calle Laakkonen

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

#include <QDebug>
#include <QSlider>
#include <QEvent>
#include <QVBoxLayout>

#include "layerwidget.h"
#include "core/layer.h"

namespace widgets {

LayerEditor::LayerEditor(const dpcore::Layer *layer, QWidget *parent)
	: QFrame(parent, Qt::FramelessWindowHint), id_(layer->id())
{
	setAttribute(Qt::WA_DeleteOnClose);
	setFrameStyle(Panel);
	setFrameShadow(Raised);

	QVBoxLayout *layout = new QVBoxLayout();
	opacity_ = new QSlider(Qt::Vertical, this);
	opacity_->setRange(0, 255);
	opacity_->setValue(layer->opacity());
	layout->addWidget(opacity_);

	setLayout(layout);

	connect(opacity_, SIGNAL(valueChanged(int)),
			this, SLOT(updateOpacity(int)));
}

LayerEditor::~LayerEditor()
{
}

void LayerEditor::changeEvent(QEvent *e)
{
	if(e->type() == QEvent::ActivationChange && !isActiveWindow()) {
		// Close the editor when focus is lost
		e->accept();
		close();
	} else
		QWidget::changeEvent(e);
}

void LayerEditor::updateOpacity(int o)
{
	emit opacityChanged(id_, o);
}

}
