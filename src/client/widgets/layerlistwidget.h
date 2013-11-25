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
#ifndef LAYERLISTWIDGET_H
#define LAYERLISTWIDGET_H

#include <QDockWidget>

class QListView;

namespace net {
	class Client;
}

namespace widgets {

class LayerListModel;
class LayerListItem;

class LayerListWidget : public QDockWidget
{
	Q_OBJECT
	public:
		LayerListWidget(QWidget *parent=0);

		LayerListModel *layerList() { return _model; }

		void setClient(net::Client *client);

		//! Initialize the widget for a new session
		void init();
		
		bool isCurrentLayerLocked() const;

	public slots:
		// State updates from the client
		void addLayer(int id, const QString &title);
		void changeLayer(int id, float opacity, const QString &title);
		void changeLayerACL(int id, bool locked, QList<uint8_t> exclusive);
		void deleteLayer(int id);
		void reorderLayers(const QList<uint8_t> &order);
		void unlockAll();

	signals:
		//! A layer was selected by the user
		void layerSelected(int id);

		//! User wants to toggle the visibility of a layer (local)
		void layerSetHidden(int id, bool hidden);
		
	private slots:
		void selected(const QModelIndex&);
		void moveLayer(int oldIdx, int newIdx);

	private:
		int currentLayer();

		net::Client *_client;
		QListView *_list;
		LayerListModel *_model;
};

}

#endif

