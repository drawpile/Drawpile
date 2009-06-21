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
#ifndef LAYERLISTWIDGET_H
#define LAYERLISTWIDGET_H

#include <QDockWidget>

class LayerListModel;
class QListView;
class QModelIndex;

namespace drawingboard {
	class Board;
}

namespace dpcore {
	class Layer;
}

class Ui_LayerBox;
class QItemSelection;

namespace widgets {

class LayerList : public QDockWidget
{
	Q_OBJECT
	public:
		LayerList(QWidget *parent=0);
		~LayerList();

		void setBoard(drawingboard::Board *board);
	
	public slots:
		void selectLayer(int id);

	signals:
		//! User wants to create a new layer
		void newLayer(const QString& name);
		//! User wants to delete a layer
		void deleteLayer(int id, bool mergedown);
		//! User wants to select a new layer
		void selected(int id);
		//! User wants to change the opacity of the currently selected layer
		void opacityChange(int id, int opacity);
		//! User wants to toggle the visibility of a layer
		void layerToggleHidden(int id);
		//! User wants to move a layer
		void layerMove(int src, int dest);
		//! User wants to rename a layer
		void renameLayer(int id, const QString& name);

	private slots:
		void newLayer();
		void deleteLayer(const dpcore::Layer* layer);
		void selected(const QItemSelection& selection, const QItemSelection& prev);
		void moved(const QModelIndex& from, const QModelIndex& to);
		void opacityChanged(int opacity);

	private:
		Ui_LayerBox *ui_;
		LayerListModel *model_;
		bool locksel_;
};

}

#endif

