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
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>

#include "layerlistwidget.h"
#include "layerlistdelegate.h"
#include "layerlistitem.h"

#include "net/client.h"

namespace widgets {

LayerListWidget::LayerListWidget(QWidget *parent)
	: QDockWidget(tr("Layers"), parent)
{
	_list = new QListView(this);
	setWidget(_list);

	_list->setDragEnabled(true);
	_list->viewport()->setAcceptDrops(true);
	_list->setEnabled(false);
	// Disallow automatic selections. We handle them ourselves in the delegate.
	_list->setSelectionMode(QAbstractItemView::NoSelection);

	LayerListDelegate *del = new LayerListDelegate(this);
	_list->setItemDelegate(del);
	
	_model = new LayerListModel(this);
	_list->setModel(_model);

	connect(del, SIGNAL(newLayer()), this, SLOT(newLayer()));
	connect(del, SIGNAL(layerSetHidden(int,bool)), this, SIGNAL(layerSetHidden(int,bool)));
	connect(del, SIGNAL(renameLayer(const QModelIndex&, const QString&)), this,
			SLOT(rename(const QModelIndex&, const QString&)));
	connect(del, SIGNAL(changeOpacity(const QModelIndex&, int)), this,
			SLOT(changeOpacity(const QModelIndex&,int)));
	connect(del, SIGNAL(select(const QModelIndex&)),
			this, SLOT(selected(const QModelIndex&)));
	connect(del, SIGNAL(deleteLayer(const QModelIndex&)), this,
			SLOT(deleteLayer(const QModelIndex&)));
	connect(_model, SIGNAL(moveLayer(int,int)), this, SLOT(moveLayer(int,int)));
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
		selected(_model->index(1,0));
	}
}

void LayerListWidget::changeLayer(int id, float opacity, const QString &title)
{
	_model->changeLayer(id, opacity, title);
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
	
	if(selection>0) {
		_list->selectionModel()->clear();
		_list->selectionModel()->select(_model->layerIndex(selection), QItemSelectionModel::Select);
	}
}

int LayerListWidget::currentLayer()
{
	QModelIndex idx = _list->selectionModel()->currentIndex();
	if(idx.isValid())
		return idx.data().value<LayerListItem>().id;
	return 0;
}

/**
 * New layer button was pressed
 */
void LayerListWidget::newLayer()
{
	bool ok;
	QString name = QInputDialog::getText(this,
		tr("Add a new layer"),
		tr("Layer name:"),
		QLineEdit::Normal,
		"",
		&ok
	);
	if(ok) {
		if(name.isEmpty())
			name = tr("Unnamed layer");
		_client->sendNewLayer(0, Qt::transparent, name);
	}
}

/**
 * New name was entered
 */
void LayerListWidget::rename(const QModelIndex &index, const QString &newtitle)
{
	LayerListItem layer = index.data().value<LayerListItem>();
	if(layer.title != newtitle) {
		layer.title = newtitle;
		sendLayerAttribs(layer);
	}
}

/**
 * Opacity slider was adjusted
 */
void LayerListWidget::changeOpacity(const QModelIndex &index, int opacity)
{
	LayerListItem layer = index.data().value<LayerListItem>();
	layer.opacity = opacity / 255.0;
	sendLayerAttribs(layer);
}

/**
 * Delete button was clicked
 */
void LayerListWidget::deleteLayer(const QModelIndex &index)
{
	Q_ASSERT(_client);
	LayerListItem layer = index.data().value<LayerListItem>();
	
	QMessageBox box(QMessageBox::Question,
		tr("Delete layer"),
		tr("Really delete \"%1\"?").arg(layer.title),
		QMessageBox::NoButton, this
	);

	box.addButton(tr("Delete"), QMessageBox::DestructiveRole);

	// Offer the choice to merge down only if there is a layer
	// below this one.
	QPushButton *merge = 0;
	if(index.sibling(index.row()+1, 0).isValid()) {
		merge = box.addButton(tr("Merge down"), QMessageBox::DestructiveRole);
		box.setInformativeText(tr("Press merge down to merge the layer with the first visible layer below instead of deleting."));
	}

	QPushButton *cancel = box.addButton(tr("Cancel"), QMessageBox::RejectRole);

	box.setDefaultButton(cancel);
	box.exec();

	QAbstractButton *choice = box.clickedButton();
	if(choice != cancel)
		_client->sendDeleteLayer(layer.id, choice==merge);
}

void LayerListWidget::sendLayerAttribs(const LayerListItem &layer)
{
	Q_ASSERT(_client);
	
	_client->sendLayerAttribs(layer.id, layer.opacity, layer.title);
}

/**
 * A layer was selected via delegate. Update the UI and emit a signal
 * to inform the Controller of the new selection.
 */
void LayerListWidget::selected(const QModelIndex& index)
{
	_list->selectionModel()->clear();
	_list->selectionModel()->select(index, QItemSelectionModel::Select);

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

