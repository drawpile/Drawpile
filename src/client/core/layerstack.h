/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2009 Calle Laakkonen

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
#ifndef LAYERSTACK_H
#define LAYERSTACK_H

#include <QList>
#include <QImage>
#include <QAbstractListModel>

namespace dpcore {

class Layer;

/**
 * A stack of layers. The stack also provides a list model so it can be
 * easily manipulated using Qt's MVC framework.
 * The model always includes one null layer which can be used as an
 * anchor point for new layers.
 */
class LayerStack : public QAbstractListModel {
	Q_OBJECT
	public:
		LayerStack(QObject *parent=0);
		~LayerStack();

		//! Initialize the image
		void init(const QSize& size);

		//! Add a new image as a layer to the top of the stack
		Layer *addLayer(const QString& name, const QImage& image, const QPoint& offset=QPoint());

		//! Add a new layer of solid color to the top of the stack
		Layer *addLayer(const QString& name, const QColor& color, const QSize& size=QSize());

		//! Add a new empty layre to the top of the stack
		Layer *addLayer(const QString& name, const QSize& size=QSize());

		//! Delete a layer
		bool deleteLayer(int id);

		//! Merge the layer to the one below it
		void mergeLayerDown(int id);

		//! Get the number of layers in the stack
		int layers() const { return layers_.count(); }

		//! Get a layer by its index
		Layer *getLayerByIndex(int index);

		//! Get a read only layer by its index
		const Layer *getLayerByIndex(int index) const;

		//! Get a layer by its ID
		Layer *getLayer(int id);

		//! Get the index of the specified layer
		int id2index(int id) const;

		//! Get the width of the layer stack
		int width() const { return width_; }

		//! Get the height of the layer stack
		int height() const { return height_; }

		//! Get the width and height of the layer stack
		QSize size() const { return QSize(width_, height_); }

		//! Paint an area of this layer stack
		void paint(const QRectF& rect, QPainter *painter);

		//! Get the merged color value at the point
		QColor colorAt(int x, int y) const;

		//! Return a flattened image of the layer stack
		QImage toFlatImage() const;

		//! Mark a tile whose content has changed
		void markDirty(int tilex, int tiley);

		//! Mark all tiles as dirty
		void markDirty();

		//! Inform the stack that a layer's properties have been changed
		void layerChanged(const Layer* layer);

		// List model functions
		int rowCount(const QModelIndex& parent) const;
		QVariant data(const QModelIndex& index, int role) const;
		Qt::ItemFlags flags(const QModelIndex& index) const;

	private:
		void flattenTile(quint32 *data, int xindex, int yindex);
		void updateCache(int xindex, int yindex);

		int width_, height_;
		int xtiles_, ytiles_;
		QList<Layer*> layers_;
		QPixmap *cache_;
		int lastid_;
};

}

Q_DECLARE_METATYPE(dpcore::Layer*)

#endif

