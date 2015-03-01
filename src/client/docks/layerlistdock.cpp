/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2015 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "net/client.h"
#include "net/layerlist.h"
#include "docks/layerlistdock.h"
#include "docks/layerlistdelegate.h"
#include "docks/layeraclmenu.h"
#include "docks/utils.h"
#include "core/rasterop.h" // for blending modes

#include "widgets/groupedtoolbutton.h"
using widgets::GroupedToolButton;
#include "ui_layerbox.h"

#include <QDebug>
#include <QItemSelection>
#include <QMessageBox>
#include <QPushButton>
#include <QActionGroup>
#include <QTimer>

namespace docks {

LayerList::LayerList(QWidget *parent)
	: QDockWidget(tr("Layers"), parent), _client(0), _selected(0), _noupdate(false), _op(false), _lockctrl(false)
{
	_ui = new Ui_LayerBox;
	QWidget *w = new QWidget(this);
	setWidget(w);
	_ui->setupUi(w);

	setStyleSheet(defaultDockStylesheet());

	_ui->layerlist->setDragEnabled(true);
	_ui->layerlist->viewport()->setAcceptDrops(true);
	_ui->layerlist->setEnabled(false);
	_ui->layerlist->setSelectionMode(QAbstractItemView::SingleSelection);

	// Populate blend mode combobox
	for(int b=0;b<paintcore::BLEND_MODES;++b) {
		if(paintcore::BLEND_MODE[b].layermode)
			_ui->blendmode->addItem(
				QApplication::translate("paintcore", paintcore::BLEND_MODE[b].name),
				paintcore::BLEND_MODE[b].id
			);
	}

	// Layer menu
	_layermenu = new QMenu(this);
	_menuHideAction = _layermenu->addAction(tr("Hide from self"), this, SLOT(hideSelected()));
	_menuHideAction->setCheckable(true);
	_menuRenameAction = _layermenu->addAction(tr("Rename"), this, SLOT(renameSelected()));
	_menuMergeAction = _layermenu->addAction(tr("Merge down"), this, SLOT(mergeSelected()));
	_menuDeleteAction = _layermenu->addAction(tr("Delete"), this, SLOT(deleteSelected()));

	// Layer ACL menu
	_aclmenu = new LayerAclMenu(this);
	_ui->lockButton->setMenu(_aclmenu);

	connect(_ui->layerlist, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(layerContextMenu(QPoint)));

	// Layer edit menu (hamburger button)
	QMenu *boxmenu = new QMenu(this);
	_addLayerAction = boxmenu->addAction(tr("New"), this, SLOT(addLayer()));
	_duplicateLayerAction = boxmenu->addAction(tr("Duplicate"), this, SLOT(duplicateLayer()));
	_deleteLayerAction = boxmenu->addAction(tr("Delete"), this, SLOT(deleteOrMergeSelected()));

#if (QT_VERSION < QT_VERSION_CHECK(5, 1, 0))
	boxmenu->addSeparator();
#else
	boxmenu->addSection(tr("View mode"));
#endif

	_viewMode = new QActionGroup(this);
	_viewMode->setExclusive(true);
	QAction *viewNormal = _viewMode->addAction(tr("Normal"));
	viewNormal->setCheckable(true);
	viewNormal->setProperty("viewmode", 0);
	QAction *viewSolo = _viewMode->addAction(tr("Solo"));
	viewSolo->setCheckable(true);
	viewSolo->setProperty("viewmode", 1);

	boxmenu->addActions(_viewMode->actions());

	_ui->menuButton->setMenu(boxmenu);

	connect(_ui->opacity, SIGNAL(valueChanged(int)), this, SLOT(opacityAdjusted()));
	connect(_ui->blendmode, SIGNAL(currentIndexChanged(int)), this, SLOT(blendModeChanged()));
	connect(_aclmenu, SIGNAL(layerAclChange(bool, QList<uint8_t>)), this, SLOT(changeLayerAcl(bool, QList<uint8_t>)));
	connect(_viewMode, SIGNAL(triggered(QAction*)), this, SLOT(layerViewModeTriggered(QAction*)));

	selectionChanged(QItemSelection());

	// The opacity update timer is used to limit the rate of layer
	// update messages sent over the network when the user drags or scrolls
	// on the opacity slider.
	_opacityUpdateTimer = new QTimer(this);
	_opacityUpdateTimer->setSingleShot(true);
	connect(_opacityUpdateTimer, SIGNAL(timeout()), this, SLOT(sendOpacityUpdate()));
}

void LayerList::setClient(net::Client *client)
{
	Q_ASSERT(_client==0);

	_client = client;
	_ui->layerlist->setModel(client->layerlist());

	LayerListDelegate *del = new LayerListDelegate(this);
	del->setClient(client);
	_ui->layerlist->setItemDelegate(del);

	_aclmenu->setUserList(client->userlist());

	connect(_client->layerlist(), SIGNAL(layerCreated(bool)), this, SLOT(onLayerCreate(bool)));
	connect(_client->layerlist(), SIGNAL(layerDeleted(int,int)), this, SLOT(onLayerDelete(int,int)));
	connect(_client->layerlist(), SIGNAL(layersReordered()), this, SLOT(onLayerReorder()));
	connect(_client->layerlist(), SIGNAL(modelReset()), this, SLOT(onLayerReorder()));
	connect(client->layerlist(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(dataChanged(QModelIndex,QModelIndex)));
	connect(_ui->layerlist->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(selectionChanged(QItemSelection)));

	connect(del, SIGNAL(toggleVisibility(int,bool)), this, SLOT(setLayerVisibility(int, bool)));
}

void LayerList::setOperatorMode(bool op)
{
	_op = op;
	updateLockedControls();
}

void LayerList::setControlsLocked(bool locked)
{
	_lockctrl = locked;
	updateLockedControls();
}

void LayerList::updateLockedControls()
{
	bool enabled = _client && (!_client->isUserLocked() & (_op | !_lockctrl));

	_addLayerAction->setEnabled(enabled);

	// Rest of the controls need a selection to work.
	// If there is a selection, but the layer is locked, the controls
	// are locked for non-operators.
	if(_selected)
		enabled = enabled & (_op | !isCurrentLayerLocked());
	else
		enabled = false;

	_ui->lockButton->setEnabled((_op && enabled) || (_client && !_client->isConnected()));
	_duplicateLayerAction->setEnabled(enabled);
	_deleteLayerAction->setEnabled(enabled);
	_ui->opacity->setEnabled(enabled);
	_ui->blendmode->setEnabled(enabled);

	_ui->layerlist->setEditTriggers(enabled ? QAbstractItemView::DoubleClicked : QAbstractItemView::NoEditTriggers);
	_menuDeleteAction->setEnabled(enabled);
	_menuMergeAction->setEnabled(enabled && canMergeCurrent());
	_menuRenameAction->setEnabled(enabled);
}

void LayerList::layerContextMenu(const QPoint &pos)
{
	QModelIndex index = _ui->layerlist->indexAt(pos);
	if(index.isValid()) {
		_layermenu->popup(_ui->layerlist->mapToGlobal(pos));
	}
}

void LayerList::selectLayer(int id)
{
	_ui->layerlist->selectionModel()->select(_client->layerlist()->layerIndex(id), QItemSelectionModel::SelectCurrent|QItemSelectionModel::Clear);
}

void LayerList::init()
{
	_ui->layerlist->setEnabled(true);
	_viewMode->actions()[0]->setChecked(true);
	setControlsLocked(false);
}

void LayerList::opacityAdjusted()
{
	// Avoid infinite loop
	if(_noupdate)
		return;

	QModelIndex index = currentSelection();
	if(index.isValid()) {
		net::LayerListItem layer = index.data().value<net::LayerListItem>();
		float opacity = _ui->opacity->value() / 255.0;
		_client->layerlist()->previewOpacityChange(layer.id, opacity);
		_opacityUpdateTimer->start(100);
	}
}

void LayerList::sendOpacityUpdate()
{
	QModelIndex index = currentSelection();
	if(index.isValid()) {
		Q_ASSERT(_client);
		net::LayerListItem layer = index.data().value<net::LayerListItem>();
		layer.opacity = _ui->opacity->value() / 255.0;
		_client->sendLayerAttribs(layer.id, layer.opacity, layer.blend);
	}
}

void LayerList::blendModeChanged()
{
	// Avoid infinite loop
	if(_noupdate)
		return;

	QModelIndex index = currentSelection();
	if(index.isValid()) {
		Q_ASSERT(_client);
		net::LayerListItem layer = index.data().value<net::LayerListItem>();
#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
		layer.blend = _ui->blendmode->currentData().toInt();
#else
		layer.blend = _ui->blendmode->itemData(_ui->blendmode->currentIndex()).toInt();
#endif
		_client->sendLayerAttribs(layer.id, layer.opacity, layer.blend);
	}
}

void LayerList::hideSelected()
{
	Q_ASSERT(_client);
	QModelIndex index = currentSelection();
	if(index.isValid()) {
		_client->sendLayerVisibility(index.data().value<net::LayerListItem>().id, _menuHideAction->isChecked());
	}
}

void LayerList::setLayerVisibility(int layerId, bool visible)
{
	Q_ASSERT(_client);
	_client->sendLayerVisibility(layerId, !visible);
}

void LayerList::changeLayerAcl(bool lock, QList<uint8_t> exclusive)
{
	Q_ASSERT(_client);
	QModelIndex index = currentSelection();
	if(index.isValid()) {
		net::LayerListItem layer = index.data().value<net::LayerListItem>();
		layer.locked = lock;
		layer.exclusive = exclusive;
		_client->sendLayerAcl(layer.id, layer.locked, layer.exclusive);
	}
}

void LayerList::layerViewModeTriggered(QAction *action)
{
	emit layerViewModeSelected(action->property("viewmode").toInt());
}

/**
 * @brief Layer add button pressed
 */
void LayerList::addLayer()
{
	const net::LayerListModel *layers = static_cast<net::LayerListModel*>(_ui->layerlist->model());

	const int id = layers->getAvailableLayerId();
	if(id==0)
		return;

	const QString name = layers->getAvailableLayerName(tr("New layer"));
	if(name.isEmpty())
		return;

	_client->sendUndopoint();
	_client->sendNewLayer(id, Qt::transparent, name);
}

void LayerList::duplicateLayer()
{
	const QModelIndex index = currentSelection();
	const net::LayerListItem layer = index.data().value<net::LayerListItem>();

	const net::LayerListModel *layers = static_cast<net::LayerListModel*>(_ui->layerlist->model());

	const int id = layers->getAvailableLayerId();
	if(id==0)
		return;

	const QString name = layers->getAvailableLayerName(layer.title);
	if(name.isEmpty())
		return;

	_client->sendUndopoint();
	_client->sendCopyLayer(layer.id, id, name);
}

bool LayerList::canMergeCurrent() const
{
	const QModelIndex index = currentSelection();
	const QModelIndex below = index.sibling(index.row()+1, 0);

	return index.isValid() && below.isValid() &&
		   !below.data().value<net::LayerListItem>().isLockedFor(_client->myId());
}

void LayerList::deleteOrMergeSelected()
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
	if(canMergeCurrent()) {
		merge = box.addButton(tr("Merge down"), QMessageBox::DestructiveRole);
		box.setInformativeText(tr("Press merge down to merge the layer with the first visible layer below instead of deleting."));
	}

	QPushButton *cancel = box.addButton(tr("Cancel"), QMessageBox::RejectRole);

	box.setDefaultButton(cancel);
	box.exec();

	QAbstractButton *choice = box.clickedButton();
	if(choice != cancel) {
		if(choice==merge)
			mergeSelected();
		else
			deleteSelected();
	}
}

void LayerList::deleteSelected()
{
	QModelIndex index = currentSelection();
	if(!index.isValid())
		return;

	_client->sendUndopoint();
	_client->sendDeleteLayer(index.data().value<net::LayerListItem>().id, false);
}

void LayerList::mergeSelected()
{
	QModelIndex index = currentSelection();
	if(!index.isValid())
		return;

	_client->sendUndopoint();
	_client->sendDeleteLayer(index.data().value<net::LayerListItem>().id, true);
}

void LayerList::renameSelected()
{
	QModelIndex index = currentSelection();
	if(!index.isValid())
		return;

	_ui->layerlist->edit(index);
}

/**
 * @brief Respond to creation of a new layer
 * @param wasfirst
 */
void LayerList::onLayerCreate(bool wasfirst)
{
	// Automatically select the first layer
	if(wasfirst)
		_ui->layerlist->selectionModel()->select(_ui->layerlist->model()->index(0,0), QItemSelectionModel::SelectCurrent);
}

/**
 * @brief Respond to layer deletion
 * @param id layer id
 * @param idx layer index in stack
 */
void LayerList::onLayerDelete(int id, int idx)
{
	Q_UNUSED(id);
	if(!currentSelection().isValid()) {
		if(idx >= _ui->layerlist->model()->rowCount())
			idx = _ui->layerlist->model()->rowCount()-1;
		else if(idx>0)
			--idx;

		// Automatically select neighbouring layer on delete
		if(idx>=0)
			_ui->layerlist->selectionModel()->select(_ui->layerlist->model()->index(idx,0), QItemSelectionModel::SelectCurrent);
		else
			_selected = 0;
	}
}

void LayerList::onLayerReorder()
{
	if(_selected)
		selectLayer(_selected);
}

QModelIndex LayerList::currentSelection() const
{
	QModelIndexList sel = _ui->layerlist->selectionModel()->selectedIndexes();
	if(sel.isEmpty())
		return QModelIndex();
	return sel.first();
}

int LayerList::currentLayer()
{
	return _selected;
}

bool LayerList::isCurrentLayerLocked() const
{
	Q_ASSERT(_client);

	QModelIndexList idx = _ui->layerlist->selectionModel()->selectedIndexes();
	if(!idx.isEmpty()) {
		const net::LayerListItem &item = idx.at(0).data().value<net::LayerListItem>();
		return item.hidden || item.isLockedFor(_client->myId());
	}
	return false;
}

void LayerList::selectionChanged(const QItemSelection &selected)
{
	bool on = selected.count() > 0;

	if(on) {
		QModelIndex cs = currentSelection();
		dataChanged(cs,cs);
		_selected = cs.data().value<net::LayerListItem>().id;
	} else {
		_selected = 0;
	}

	updateLockedControls();

	emit layerSelected(_selected);
}

void LayerList::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
	const int myRow = currentSelection().row();
	if(topLeft.row() <= myRow && myRow <= bottomRight.row()) {
		const net::LayerListItem &layer = currentSelection().data().value<net::LayerListItem>();
		_noupdate = true;
		_menuHideAction->setChecked(layer.hidden);
		_ui->opacity->setValue(layer.opacity * 255);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
		int blendmode = _ui->blendmode->currentData().toInt();
#else
		int blendmode = _ui->blendmode->itemData(_ui->blendmode->currentIndex()).toInt();
#endif
		if(blendmode != layer.blend) {
			for(int i=0;i<_ui->blendmode->count();++i) {
				if(_ui->blendmode->itemData(i).toInt() == layer.blend) {
				_ui->blendmode->setCurrentIndex(i);
				break;
				}
			}
		}

		_ui->lockButton->setChecked(layer.locked || !layer.exclusive.isEmpty());
		_aclmenu->setAcl(layer.locked, layer.exclusive);
		updateLockedControls();
		_noupdate = false;
	}
}

}
