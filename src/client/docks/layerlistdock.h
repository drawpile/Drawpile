/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2013 Calle Laakkonen

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
#ifndef LAYERLISTDOCK_H
#define LAYERLISTDOCK_H

#include <QDockWidget>

class QListView;

namespace net {
	class Client;
}

namespace widgets {

class LayerListDock : public QDockWidget
{
	Q_OBJECT
	public:
		LayerListDock(QWidget *parent=0);

		void setClient(net::Client *client);

		//! Initialize the widget for a new session
		void init();
		
		//! Get the ID of the currently selected layer
		int currentLayer();

		bool isCurrentLayerLocked() const;

	signals:
		//! A layer was selected by the user
		void layerSelected(int id);
		
	private slots:
		void selected(const QModelIndex&);
		void onLayerCreate(bool wasfirst);
		void onLayerDelete(int id, int idx);
		void onLayerReorder();

	private:

		net::Client *_client;
		QListView *_list;
		int _selected;
};

}

#endif

