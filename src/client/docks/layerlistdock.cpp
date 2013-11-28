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
#include <QDebug>
#include <QItemSelection>
#include <QListView>

#include "net/client.h"
#include "net/layerlist.h"
#include "docks/layerlistdock.h"
#include "docks/layerlistdelegate.h"

namespace widgets {

LayerListDock::LayerListDock(QWidget *parent)
	: QDockWidget(tr("Layers"), parent), _client(0), _selected(0)
{
	_list = new QListView(this);
	setWidget(_list);

	_list->setDragEnabled(true);
	_list->viewport()->setAcceptDrops(true);
	_list->setEnabled(false);
	// Disallow automatic selections. We handle them ourselves in the delegate.
	_list->setSelectionMode(QAbstractItemView::NoSelection);
}

void LayerListDock::setClient(net::Client *client)
{
	Q_ASSERT(_client==0);

	_client = client;
	_list->setModel(client->layerlist());

	LayerListDelegate *del = new LayerListDelegate(this);
	del->setClient(client);
	_list->setItemDelegate(del);

	connect(del, SIGNAL(select(const QModelIndex&)), this, SLOT(selected(const QModelIndex&)));

	connect(_client->layerlist(), SIGNAL(layerCreated(bool)), this, SLOT(onLayerCreate(bool)));
	connect(_client->layerlist(), SIGNAL(layerDeleted(int,int)), this, SLOT(onLayerDelete(int,int)));
	connect(_client->layerlist(), SIGNAL(layersReordered()), this, SLOT(onLayerReorder()));
}

void LayerListDock::init()
{
	_list->setEnabled(true);
}

void LayerListDock::onLayerCreate(bool wasfirst)
{
	// Automatically select the first layer
	if(wasfirst)
		selected(_list->model()->index(1, 0));
}

void LayerListDock::onLayerDelete(int id, int idx)
{
	// Automatically select the neighbouring layer on delete
	if(_selected == id) {
		if(_list->model()->rowCount() <= idx)
			--idx;
		if(idx>0)
			selected(_list->model()->index(idx, 0));
		else
			_selected = 0;
	}
}

void LayerListDock::onLayerReorder()
{
	if(_selected) {
		_list->selectionModel()->clear();
		_list->selectionModel()->select(_client->layerlist()->layerIndex(_selected), QItemSelectionModel::SelectCurrent);
	}
}

int LayerListDock::currentLayer()
{
	return _selected;
}

bool LayerListDock::isCurrentLayerLocked() const
{
	Q_ASSERT(_client);

	QModelIndexList idx = _list->selectionModel()->selectedIndexes();
	if(!idx.isEmpty())
		return idx.at(0).data().value<net::LayerListItem>().isLockedFor(_client->myId());
	return false;
}

/**
 * A layer was selected via delegate. Update the UI and emit a signal
 * to inform the Controller of the new selection.
 */
void LayerListDock::selected(const QModelIndex& index)
{
	_list->selectionModel()->clear();
	_list->selectionModel()->select(index, QItemSelectionModel::SelectCurrent);

	_selected = index.data().value<net::LayerListItem>().id;
	emit layerSelected(_selected);
}

}

