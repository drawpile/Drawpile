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
#ifndef LAYERLISTMODEL_H
#define LAYERLISTMODEL_H

#include <QAbstractListModel>
#include <QItemDelegate>

namespace dpcore {
	class Layer;
}

/**
 * A custom item delegate for displaying layer names
 * and editing layer settings.
 * User actions cause signals to be emitted, which are routed to Controller,
 * which either acts upon them or sends them to the server. In any case,
 * we don't modify anything directly here at all.
 */
class LayerListDelegate : public QItemDelegate {
	Q_OBJECT
	public:
		LayerListDelegate(QObject *parent=0);
		~LayerListDelegate();

		void paint(QPainter *painter, const QStyleOptionViewItem &option,
				const QModelIndex &index) const;
		QSize sizeHint(const QStyleOptionViewItem & option,
				const QModelIndex & index ) const;
		bool editorEvent(QEvent *event, QAbstractItemModel *model,
				const QStyleOptionViewItem &option, const QModelIndex &index);

		void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
		void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex& index) const;

	signals:
		//! Create new layer button was pressed
		void newLayer();
		//! User request the given layer to be deleted
		void deleteLayer(const dpcore::Layer *layer);
		//! User wants to toggle the visibility of the given layer
		void layerToggleHidden(int);
		//! User wants to rename the layer
		void renameLayer(int id, const QString& name) const;
		//! User wants to change the layer's opacity
		void changeOpacity(int id, int opacity);
		//! User wants to select the layer
		void select(const QModelIndex& index);
	
	private:
		void drawMeter(const QRectF& rect, QPainter *painter, const QColor& background, const QColor& foreground, float value) const;
};

#endif

