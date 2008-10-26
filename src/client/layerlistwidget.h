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
#ifndef LAYERLISTWIDGET_H
#define LAYERLISTWIDGET_H

#include <QDockWidget>

class LayerListModel;
class QListView;

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
		void newLayer(const QString& name);
		void deleteLayer(int id, bool mergedown);
		void selected(int id);

	private slots:
		void newLayer();
		void deleteLayer(const dpcore::Layer* layer);
		void selected(const QItemSelection& selection, const QItemSelection& prev);

	private:
		Ui_LayerBox *ui_;
		LayerListModel *model_;
};

}

#endif

