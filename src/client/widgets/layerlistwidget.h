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

class QListView;

namespace protocol {
	class Message;
}

namespace widgets {

class LayerListModel;

class LayerListWidget : public QDockWidget
{
	Q_OBJECT
	public:
		LayerListWidget(QWidget *parent=0);

		LayerListModel *layerList() { return _model; }

	public slots:
		void setSelection(int id);
		
	signals:
		//! Layer altering command emitted in response to user input
		void layerCommand(protocol::Message *cmd);

		//! User wants to toggle the visibility of a layer (local)
		void layerHide(int id, bool hidden);
		
	private:
		QListView *_list;
		LayerListModel *_model;
};

}

#endif

