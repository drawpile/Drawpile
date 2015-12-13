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
#ifndef LAYERSTACKITEM_H
#define LAYERSTACKITEM_H

#include "core/layerstack.h"

//#include <QQuickItem>
#include <QQuickPaintedItem> // transitional

#include <QPixmap>
#include <QPointer>

class LayerStackItem : public QQuickPaintedItem
{
	Q_PROPERTY(paintcore::LayerStack* model READ model WRITE setModel NOTIFY modelChanged)

	Q_OBJECT
public:
	explicit LayerStackItem(QQuickItem *parent=0);

	void setModel(paintcore::LayerStack *model);
	paintcore::LayerStack *model() const { return m_model.data(); }

	void paint(QPainter *painter);

signals:
	void modelChanged();

private slots:
	void onLayerStackResize(int xoffset, int yoffset, const QSize &oldsize);

private:
	QPointer<paintcore::LayerStack> m_model;
	QPixmap m_cache;
};

#endif // CANVASITEM_H
