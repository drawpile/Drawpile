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

#include "layerlistwidget.h"
#include "layerlistdelegate.h"
#include "layerlistitem.h"

#include "net/client.h"

namespace widgets {

LayerListWidget::LayerListWidget(QWidget *parent)
	: QDockWidget(tr("Layers"), parent), _client(0)
{
	_list = new QListView(this);
	setWidget(_list);

	_list->setDragEnabled(true);
	_list->viewport()->setAcceptDrops(true);
	_list->setEnabled(false);
	// Disallow automatic selections. We handle them ourselves in the delegate.
	_list->setSelectionMode(QAbstractItemView::NoSelection);
	
	_model = new LayerListModel(this);
	_list->setModel(_model);

	connect(_model, SIGNAL(moveLayer(int,int)), this, SLOT(moveLayer(int,int)));
}

void LayerListWidget::setClient(net::Client *client)
{
	Q_ASSERT(_client==0);

	_client = client;
	LayerListDelegate *del = new LayerListDelegate(this);
	del->setClient(client);
	_list->setItemDelegate(del);

	connect(del, SIGNAL(layerSetHidden(int,bool)), this, SIGNAL(layerSetHidden(int,bool)));
	connect(del, SIGNAL(select(const QModelIndex&)), this, SLOT(selected(const QModelIndex&)));
}

void LayerListWidget::init()
{
	_model->clear();
	_list->setEnabled(true);
}

void LayerListWidget::addLayer(int id, const QString &title)
{
	_model->createLayer(id, title);
	if(_model->rowCount()==2) {
		// Automatically select the first layer
		selected(_model->index(1));
	}
}

void LayerListWidget::changeLayer(int id, float opacity, const QString &title)
{
	_model->changeLayer(id, opacity, title);
}

void LayerListWidget::changeLayerACL(int id, bool locked, QList<uint8_t> exclusive)
{
	_model->updateLayerAcl(id, locked, exclusive);
}

void LayerListWidget::unlockAll()
{
	_model->unlockAll();
}

void LayerListWidget::deleteLayer(int id)
{
	_model->deleteLayer(id);
	// TODO change layer if this one was selected
}

void LayerListWidget::reorderLayers(const QList<uint8_t> &order)
{
	int selection = currentLayer();

	_model->reorderLayers(order);
	
	if(selection) {
		_list->selectionModel()->clear();
		_list->selectionModel()->select(_model->layerIndex(selection), QItemSelectionModel::SelectCurrent);
	}
}

int LayerListWidget::currentLayer()
{
	QModelIndexList selidx = _list->selectionModel()->selectedIndexes();
	if(!selidx.isEmpty())
		return selidx.at(0).data().value<LayerListItem>().id;
	return 0;
}

bool LayerListWidget::isCurrentLayerLocked() const
{
	QModelIndexList idx = _list->selectionModel()->selectedIndexes();
	if(!idx.isEmpty())
		return idx.at(0).data().value<LayerListItem>().isLockedFor(_client->myId());
	return false;
}

/**
 * A layer was selected via delegate. Update the UI and emit a signal
 * to inform the Controller of the new selection.
 */
void LayerListWidget::selected(const QModelIndex& index)
{
	_list->selectionModel()->clear();
	_list->selectionModel()->select(index, QItemSelectionModel::SelectCurrent);

	emit layerSelected(index.data().value<LayerListItem>().id);
}

void LayerListWidget::moveLayer(int oldIdx, int newIdx)
{
	// Need at least two real layers for this to make sense
	if(_model->rowCount() <= 2)
		return;
	
	QList<uint8_t> layers;
	int rows = _model->rowCount() - 1;
	for(int i=rows;i>=1;--i)
		layers.append(_model->data(_model->index(i, 0)).value<LayerListItem>().id);
	
	int m0 = rows-oldIdx-1;
	int m1 = rows-newIdx;
	if(m1>m0) --m1;
	
	if(m0 == m1)
		return;

	layers.move(m0, m1);

	_client->sendLayerReorder(layers);
}

}

