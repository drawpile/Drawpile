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
#include <QMessageBox>
#include <QInputDialog>
#include <QPushButton>

#include "net/client.h"
#include "net/layerlist.h"
#include "docks/layerlistdock.h"
#include "docks/layerlistdelegate.h"
#include "core/rasterop.h" // for blending modes

#include "ui_layerbox.h"

namespace widgets {

LayerListDock::LayerListDock(QWidget *parent)
	: QDockWidget(tr("Layers"), parent), _client(0), _selected(0), _noupdate(false)
{
	_ui = new Ui_LayerBox;
	QWidget *w = new QWidget(this);
	setWidget(w);
	_ui->setupUi(w);

	_ui->layerlist->setDragEnabled(true);
	_ui->layerlist->viewport()->setAcceptDrops(true);
	_ui->layerlist->setEnabled(false);
	_ui->addButton->setEnabled(false);
	_ui->layerlist->setSelectionMode(QAbstractItemView::SingleSelection);

	// Populate blend mode combobox
	for(int b=0;b<dpcore::BLEND_MODES;++b) {
		_ui->blendmode->addItem(QApplication::tr(dpcore::BLEND_MODE[b]));
	}

	connect(_ui->addButton, SIGNAL(clicked()), this, SLOT(addLayer()));
	connect(_ui->deleteButton, SIGNAL(clicked()), this, SLOT(deleteSelected()));
	connect(_ui->hideButton, SIGNAL(clicked()), this, SLOT(hiddenToggled()));
	connect(_ui->opacity, SIGNAL(valueChanged(int)), this, SLOT(opacityAdjusted()));
	connect(_ui->blendmode, SIGNAL(currentIndexChanged(int)), this, SLOT(blendModeChanged()));
	connect(_ui->lockButton, SIGNAL(clicked()), this, SLOT(lockSelected()));

	selectionChanged(QItemSelection());
}

void LayerListDock::setClient(net::Client *client)
{
	Q_ASSERT(_client==0);

	_client = client;
	_ui->layerlist->setModel(client->layerlist());

	LayerListDelegate *del = new LayerListDelegate(this);
	del->setClient(client);
	_ui->layerlist->setItemDelegate(del);

	connect(_client->layerlist(), SIGNAL(layerCreated(bool)), this, SLOT(onLayerCreate(bool)));
	connect(_client->layerlist(), SIGNAL(layerDeleted(int,int)), this, SLOT(onLayerDelete(int,int)));
	connect(_client->layerlist(), SIGNAL(layersReordered()), this, SLOT(onLayerReorder()));

	connect(client->layerlist(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(dataChanged(QModelIndex,QModelIndex)));
	connect(_ui->layerlist->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(selectionChanged(QItemSelection)));
}

void LayerListDock::init()
{
	_ui->layerlist->setEnabled(true);
	_ui->addButton->setEnabled(true);
}

void LayerListDock::opacityAdjusted()
{
	// Avoid infinite loop
	if(_noupdate)
		return;

	QModelIndex index = currentSelection();
	if(index.isValid()) {
		Q_ASSERT(_client);
		net::LayerListItem layer = index.data().value<net::LayerListItem>();
		layer.opacity = _ui->opacity->value() / 255.0;
		_client->sendLayerAttribs(layer.id, layer.opacity, layer.blend);
	}
}

void LayerListDock::blendModeChanged()
{
	// Avoid infinite loop
	if(_noupdate)
		return;

	QModelIndex index = currentSelection();
	if(index.isValid()) {
		Q_ASSERT(_client);
		net::LayerListItem layer = index.data().value<net::LayerListItem>();
		layer.blend = _ui->blendmode->currentIndex();
		_client->sendLayerAttribs(layer.id, layer.opacity, layer.blend);
	}
}

void LayerListDock::hiddenToggled()
{
	QModelIndex index = currentSelection();
	if(index.isValid()) {
		net::LayerListItem layer = index.data().value<net::LayerListItem>();
		_client->sendLayerVisibility(layer.id, _ui->hideButton->isChecked());
	}
}

void LayerListDock::lockSelected()
{
	Q_ASSERT(_client);
	QModelIndex index = currentSelection();
	if(index.isValid()) {
		Q_ASSERT(_client);
		net::LayerListItem layer = index.data().value<net::LayerListItem>();
		layer.locked = _ui->lockButton->isChecked();
		_client->sendLayerAcl(layer.id, layer.locked, layer.exclusive);
	}
}

/**
 * @brief Layer add button pressed
 */
void LayerListDock::addLayer()
{
	bool ok;
	QString name = QInputDialog::getText(0,
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
 * @brief Layer delete button pressed
 */
void LayerListDock::deleteSelected()
{
	Q_ASSERT(_client);
	QModelIndex index = currentSelection();
	if(!index.isValid())
		return;

	net::LayerListItem layer = index.data().value<net::LayerListItem>();

	QMessageBox box(QMessageBox::Question,
		tr("Delete layer"),
		tr("Really delete \"%1\"?").arg(layer.title),
		QMessageBox::NoButton
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

/**
 * @brief Respond to creation of a new layer
 * @param wasfirst
 */
void LayerListDock::onLayerCreate(bool wasfirst)
{
	// Automatically select the first layer
	if(wasfirst)
		_ui->layerlist->selectionModel()->select(_ui->layerlist->model()->index(0,0), QItemSelectionModel::SelectCurrent);
}

/**
 * @brief Respond to layer deletion
 * @param id
 * @param idx
 */
void LayerListDock::onLayerDelete(int id, int idx)
{
	// Automatically select the neighbouring layer on delete
	if(_selected == id) {
		if(_ui->layerlist->model()->rowCount() <= idx)
			--idx;
		if(idx>0)
			_ui->layerlist->selectionModel()->select(_ui->layerlist->model()->index(idx,0), QItemSelectionModel::SelectCurrent);
		else
			_selected = 0;
	}
}

void LayerListDock::onLayerReorder()
{
	if(_selected) {
		_ui->layerlist->selectionModel()->clear();
		_ui->layerlist->selectionModel()->select(_client->layerlist()->layerIndex(_selected), QItemSelectionModel::SelectCurrent);
	}
}

QModelIndex LayerListDock::currentSelection()
{
	QModelIndexList sel = _ui->layerlist->selectionModel()->selectedIndexes();
	if(sel.isEmpty())
		return QModelIndex();
	return sel.first();
}

int LayerListDock::currentLayer()
{
	return _selected;
}

bool LayerListDock::isCurrentLayerLocked() const
{
	Q_ASSERT(_client);

	QModelIndexList idx = _ui->layerlist->selectionModel()->selectedIndexes();
	if(!idx.isEmpty())
		return idx.at(0).data().value<net::LayerListItem>().isLockedFor(_client->myId());
	return false;
}

void LayerListDock::selectionChanged(const QItemSelection &selected)
{
	bool on = selected.count() > 0;
	_ui->hideButton->setEnabled(on);
	_ui->opacity->setEnabled(on);
	_ui->lockButton->setEnabled(on);
	_ui->deleteButton->setEnabled(on);

	if(on) {
		QModelIndex cs = currentSelection();
		dataChanged(cs,cs);
		_selected = cs.data().value<net::LayerListItem>().id;
	} else {
		_selected = 0;
	}
	emit layerSelected(_selected);
}

void LayerListDock::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
	const int myRow = currentSelection().row();
	if(topLeft.row() <= myRow && myRow <= bottomRight.row()) {
		const net::LayerListItem &layer = currentSelection().data().value<net::LayerListItem>();
		_noupdate = true;
		_ui->hideButton->setChecked(layer.hidden);
		_ui->opacity->setValue(layer.opacity * 255);
		_ui->lockButton->setChecked(layer.locked);
		_ui->blendmode->setCurrentIndex(layer.blend);
		_noupdate = false;
	}
}

}
