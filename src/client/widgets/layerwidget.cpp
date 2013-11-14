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
#include <QCheckBox>
#include <QEvent>
#include <QLabel>
#include <QGridLayout>

#include "layerwidget.h"
#include "core/layer.h"

namespace widgets {

LayerStyleEditor::LayerStyleEditor(const dpcore::Layer *layer, QWidget *parent)
	: QFrame(parent, Qt::FramelessWindowHint), id_(layer->id())
{
	setAttribute(Qt::WA_DeleteOnClose);
	setFrameStyle(Panel);
	setFrameShadow(Raised);

	QGridLayout *layout = new QGridLayout();

	hide_ = new QCheckBox(this);
	hide_->setChecked(layer->hidden());
	hide_->setText(tr("Hide"));
	layout->addWidget(hide_, 0, 0);

	QLabel *lbl = new QLabel(this);
	lbl->setText(tr("Opacity") + ":");
	layout->addWidget(lbl, 1, 0);

	opacity_ = new QSlider(Qt::Horizontal, this);
	opacity_->setRange(0, 255);
	opacity_->setValue(layer->opacity());
	layout->addWidget(opacity_, 1, 1);

	setLayout(layout);

	connect(opacity_, SIGNAL(valueChanged(int)),
			this, SLOT(updateOpacity(int)));
	connect(hide_, SIGNAL(toggled(bool)),
			this, SLOT(toggleHide()));
}

LayerStyleEditor::~LayerStyleEditor()
{
}

void LayerStyleEditor::changeEvent(QEvent *e)
{
	if(e->type() == QEvent::ActivationChange && !isActiveWindow()) {
		// Close the editor when focus is lost
		e->accept();
		close();
	} else
		QWidget::changeEvent(e);
}

void LayerStyleEditor::updateOpacity(int o)
{
	emit opacityChanged(id_, o);
}

void LayerStyleEditor::toggleHide()
{
	emit toggleHidden(id_);
}

}
