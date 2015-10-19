/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#include "layerstackitem.h"
#include "canvas/canvasmodel.h"

#include "core/layerstack.h"

#include <QPainter>
#include <QDebug>

LayerStackItem::LayerStackItem(QQuickItem *parent)
	: QQuickPaintedItem(parent)
{
}

void LayerStackItem::setModel(paintcore::LayerStack *model)
{
	if(model != m_model.data()) {

		// Disconnect previous model
		if(m_model) {
			disconnect(m_model, &paintcore::LayerStack::resized, this, &LayerStackItem::onLayerStackResize);
			disconnect(m_model, &paintcore::LayerStack::areaChanged, this, &LayerStackItem::update);
		}

		m_model = model;

		// Connect new model
		connect(model, &paintcore::LayerStack::resized, this, &LayerStackItem::onLayerStackResize);
		connect(model, &paintcore::LayerStack::areaChanged, this, &LayerStackItem::update);

		emit modelChanged();
	}
}

paintcore::AnnotationModel *LayerStackItem::annotations() const
{
	return m_model ? m_model->annotations() : nullptr;
}

void LayerStackItem::paint(QPainter *painter)
{
	paintcore::LayerStack::Locker locker(m_model);

	if(m_cache.isNull())
		m_cache = QPixmap(m_model->size());

	m_model->paintChangedTiles(QRect(QPoint(), m_cache.size()), &m_cache);

	painter->drawPixmap(0, 0, m_cache);
}

void LayerStackItem::onLayerStackResize(int xoffset, int yoffset, const QSize &oldsize)
{
	Q_UNUSED(xoffset);
	Q_UNUSED(yoffset);
	Q_UNUSED(oldsize);
	setImplicitWidth(m_model->width());
	setImplicitHeight(m_model->height());
	m_cache = QPixmap();
}
