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

#include "canvas/layerlist.h"
#include "canvas/aclfilter.h"
#include "canvas/canvasmodel.h"
#include "docks/layerlistdock.h"
#include "docks/layerlistdelegate.h"
#include "docks/layeraclmenu.h"
#include "docks/utils.h"
#include "core/blendmodes.h"

#include "../shared/net/layer.h"
#include "../shared/net/meta2.h"
#include "../shared/net/undo.h"

#include "widgets/groupedtoolbutton.h"
using widgets::GroupedToolButton;
#include "ui_layerbox.h"

#include <QDebug>
#include <QItemSelection>
#include <QMessageBox>
#include <QPushButton>
#include <QActionGroup>
#include <QTimer>
#include <QSettings>

namespace docks {

LayerList::LayerList(QWidget *parent)
	: QDockWidget(tr("Layers"), parent), m_canvas(nullptr), m_selectedId(0), _noupdate(false), m_op(false), m_lockctrl(false), m_ownlayers(false)
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
	for(auto bm : paintcore::getBlendModeNames(paintcore::BlendMode::LayerMode))
		_ui->blendmode->addItem(bm.second, bm.first);

	// Layer menu
	_layermenu = new QMenu(this);
	_menuInsertAction = _layermenu->addAction(tr("Insert layer"), this, SLOT(insertLayer()));
	_layermenu->addSeparator();
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

	QActionGroup *viewmodes = new QActionGroup(this);
	viewmodes->setExclusive(true);

	QAction *viewNormal = viewmodes->addAction(tr("Normal"));
	viewNormal->setCheckable(true);
	viewNormal->setProperty("viewmode", 0);

	QAction *viewSolo = viewmodes->addAction(tr("Solo"));
	viewSolo->setCheckable(true);
	viewSolo->setProperty("viewmode", 1);

	QAction *viewOnionDown = viewmodes->addAction(tr("Onionskin"));
	viewOnionDown->setCheckable(true);
	viewOnionDown->setProperty("viewmode", 2);

	boxmenu->addSeparator();
	_viewMode = boxmenu->addMenu(QString()); // title is set later
	_viewMode->addActions(viewmodes->actions());

	_showNumbersAction = boxmenu->addAction(tr("Show numbers"));
	_showNumbersAction->setCheckable(true);

	_ui->menuButton->setMenu(boxmenu);

	connect(_ui->opacity, SIGNAL(valueChanged(int)), this, SLOT(opacityAdjusted()));
	connect(_ui->blendmode, SIGNAL(currentIndexChanged(int)), this, SLOT(blendModeChanged()));
	connect(_aclmenu, SIGNAL(layerAclChange(bool, QList<uint8_t>)), this, SLOT(changeLayerAcl(bool, QList<uint8_t>)));
	connect(_viewMode, SIGNAL(triggered(QAction*)), this, SLOT(layerViewModeTriggered(QAction*)));
	connect(_showNumbersAction, &QAction::triggered, this, &LayerList::showLayerNumbers);

	selectionChanged(QItemSelection());

	// The opacity update timer is used to limit the rate of layer
	// update messages sent over the network when the user drags or scrolls
	// on the opacity slider.
	_opacityUpdateTimer = new QTimer(this);
	_opacityUpdateTimer->setSingleShot(true);
	connect(_opacityUpdateTimer, SIGNAL(timeout()), this, SLOT(sendOpacityUpdate()));
}

void LayerList::setCanvas(canvas::CanvasModel *canvas)
{
	m_canvas = canvas;
	_ui->layerlist->setModel(canvas->layerlist());

	LayerListDelegate *del = new LayerListDelegate(this);
	connect(del, SIGNAL(toggleVisibility(int,bool)), this, SLOT(setLayerVisibility(int, bool)));
	_ui->layerlist->setItemDelegate(del);

	_aclmenu->setUserList(canvas->userlist());

	connect(canvas->layerlist(), &canvas::LayerListModel::rowsInserted, this, &LayerList::onLayerCreate);
	connect(canvas->layerlist(), &canvas::LayerListModel::rowsAboutToBeRemoved, this, &LayerList::onLayerDelete);
	connect(canvas->layerlist(), SIGNAL(layersReordered()), this, SLOT(onLayerReorder()));
	connect(canvas->layerlist(), SIGNAL(modelReset()), this, SLOT(onLayerReorder()));
	connect(canvas->layerlist(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(dataChanged(QModelIndex,QModelIndex)));
	connect(canvas->aclFilter(), &canvas::AclFilter::layerControlLockChanged, this, &LayerList::setControlsLocked);
	connect(canvas->aclFilter(), &canvas::AclFilter::ownLayersChanged, this, &LayerList::setOwnLayers);
	connect(_ui->layerlist->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(selectionChanged(QItemSelection)));

	// Restore settings
	QSettings cfg;
	_showNumbersAction->setChecked(cfg.value("setting/layernumbers", false).toBool());
	del->setShowNumbers(_showNumbersAction->isChecked());
}

void LayerList::setOperatorMode(bool op)
{
	m_op = op;
	updateLockedControls();
}

void LayerList::setControlsLocked(bool locked)
{
	m_lockctrl = locked;
	updateLockedControls();
}

void LayerList::setOwnLayers(bool own)
{
	m_ownlayers = own;
	updateLockedControls();
}

void LayerList::updateLockedControls()
{
	bool enabled = m_canvas && m_canvas->aclFilter()->canUseLayerControls(currentLayer());

	_addLayerAction->setEnabled(m_canvas && m_canvas->aclFilter()->canCreateLayer());
	_menuInsertAction->setEnabled(enabled);

	// Rest of the controls need a selection to work.
	// If there is a selection, but the layer is locked, the controls
	// are locked for non-operators.
	if(m_selectedId)
		enabled = enabled & (m_op | !isCurrentLayerLocked());
	else
		enabled = false;

	_ui->lockButton->setEnabled(
		m_op ||
		(m_canvas && !m_canvas->isOnline()) ||
		(m_ownlayers && m_canvas && (currentLayer()>>8) == m_canvas->localUserId())
	);
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
	_ui->layerlist->selectionModel()->select(m_canvas->layerlist()->layerIndex(id), QItemSelectionModel::SelectCurrent|QItemSelectionModel::Clear);
}

void LayerList::selectAbove()
{
	QModelIndex current = currentSelection();
	QModelIndex prev = current.sibling(current.row() - 1, 0);
	if(prev.isValid())
		_ui->layerlist->selectionModel()->select(prev, QItemSelectionModel::SelectCurrent|QItemSelectionModel::Clear);
}

void LayerList::selectBelow()
{
	QModelIndex current = currentSelection();
	QModelIndex prev = current.sibling(current.row() + 1, 0);
	if(prev.isValid())
		_ui->layerlist->selectionModel()->select(prev, QItemSelectionModel::SelectCurrent|QItemSelectionModel::Clear);
}

void LayerList::init()
{
	_ui->layerlist->setEnabled(true);
	_viewMode->actions()[0]->setChecked(true);
	layerViewModeTriggered(_viewMode->actions()[0]);
	setControlsLocked(false);
}

void LayerList::opacityAdjusted()
{
	// Avoid infinite loop
	if(_noupdate)
		return;

	QModelIndex index = currentSelection();
	if(index.isValid()) {
		int layerId = index.data(canvas::LayerListModel::IdRole).toInt();
		float opacity = _ui->opacity->value() / 255.0;

		m_canvas->layerlist()->previewOpacityChange(layerId, opacity);
		_opacityUpdateTimer->start(100);
	}
}

void LayerList::sendOpacityUpdate()
{
	QModelIndex index = currentSelection();
	if(index.isValid()) {
		paintcore::LayerInfo layer = index.data().value<paintcore::LayerInfo>();
		emit layerCommand(protocol::MessagePtr(new protocol::LayerAttributes(0, layer.id, _ui->opacity->value(), int(layer.blend))));
	}
}

void LayerList::blendModeChanged()
{
	// Avoid infinite loop
	if(_noupdate)
		return;

	QModelIndex index = currentSelection();
	if(index.isValid()) {
		paintcore::LayerInfo layer = index.data().value<paintcore::LayerInfo>();
		emit layerCommand(protocol::MessagePtr(new protocol::LayerAttributes(0, layer.id, layer.opacity, _ui->blendmode->currentData().toInt())));
	}
}

void LayerList::hideSelected()
{
	QModelIndex index = currentSelection();
	if(index.isValid())
		setLayerVisibility(index.data(canvas::LayerListModel::IdRole).toInt(), _menuHideAction->isChecked());
}

void LayerList::setLayerVisibility(int layerId, bool visible)
{
	emit layerCommand(protocol::MessagePtr(new protocol::LayerVisibility(0, layerId, visible)));
}

void LayerList::changeLayerAcl(bool lock, QList<uint8_t> exclusive)
{
	QModelIndex index = currentSelection();
	if(index.isValid()) {
		paintcore::LayerInfo layer = index.data().value<paintcore::LayerInfo>();
		layer.locked = lock;
		layer.exclusive = exclusive;
		emit layerCommand(protocol::MessagePtr(new protocol::LayerACL(0, layer.id, layer.locked, layer.exclusive)));
	}
}

void LayerList::layerViewModeTriggered(QAction *action)
{
	_viewMode->setTitle(tr("Mode:") + " " + action->text());
	emit layerViewModeSelected(action->property("viewmode").toInt());
}

void LayerList::showLayerNumbers(bool show)
{
	LayerListDelegate *del = static_cast<LayerListDelegate*>(_ui->layerlist->itemDelegate());
	del->setShowNumbers(show);

	QSettings cfg;
	cfg.setValue("setting/layernumbers", _showNumbersAction->isChecked());

}

/**
 * @brief Layer add button pressed
 */
void LayerList::addLayer()
{
	const canvas::LayerListModel *layers = static_cast<canvas::LayerListModel*>(_ui->layerlist->model());

	const int id = layers->getAvailableLayerId();
	if(id==0)
		return;

	const QString name = layers->getAvailableLayerName(tr("Layer"));

	emit layerCommand(protocol::MessagePtr(new protocol::UndoPoint(0)));
	emit layerCommand(protocol::MessagePtr(new protocol::LayerCreate(0, id, 0, 0, 0, name)));
}

/**
 * @brief Insert a new layer above the current selection
 */
void LayerList::insertLayer()
{
	const QModelIndex index = currentSelection();
	const int layerId = index.data(canvas::LayerListModel::IdRole).toInt();

	const canvas::LayerListModel *layers = static_cast<canvas::LayerListModel*>(_ui->layerlist->model());

	const int id = layers->getAvailableLayerId();
	if(id==0)
		return;

	const QString name = layers->getAvailableLayerName(tr("Layer"));

	emit layerCommand(protocol::MessagePtr(new protocol::UndoPoint(0)));
	emit layerCommand(protocol::MessagePtr(new protocol::LayerCreate(0, id, layerId, 0, protocol::LayerCreate::FLAG_INSERT, name)));
}

void LayerList::duplicateLayer()
{
	const QModelIndex index = currentSelection();
	const int layerId = index.data(canvas::LayerListModel::IdRole).toInt();
	const QString layerTitle = index.data(canvas::LayerListModel::TitleRole).toString();

	const canvas::LayerListModel *layers = static_cast<canvas::LayerListModel*>(_ui->layerlist->model());

	const int id = layers->getAvailableLayerId();
	if(id==0)
		return;

	const QString name = layers->getAvailableLayerName(layerTitle);

	emit layerCommand(protocol::MessagePtr(new protocol::UndoPoint(0)));
	emit layerCommand(protocol::MessagePtr(new protocol::LayerCreate(0, id, layerId, 0, protocol::LayerCreate::FLAG_INSERT | protocol::LayerCreate::FLAG_COPY, name)));
}

bool LayerList::canMergeCurrent() const
{
	const QModelIndex index = currentSelection();
	const QModelIndex below = index.sibling(index.row()+1, 0);

	return index.isValid() && below.isValid() &&
		   !below.data().value<paintcore::LayerInfo>().isLockedFor(m_canvas->localUserId());
}

void LayerList::deleteOrMergeSelected()
{
	QModelIndex index = currentSelection();
	if(!index.isValid())
		return;

	const QString layerTitle = index.data(canvas::LayerListModel::TitleRole).toString();

	QMessageBox box(QMessageBox::Question,
		tr("Delete layer"),
		tr("Really delete \"%1\"?").arg(layerTitle),
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

	emit layerCommand(protocol::MessagePtr(new protocol::UndoPoint(0)));
	emit layerCommand(protocol::MessagePtr(new protocol::LayerDelete(0, index.data(canvas::LayerListModel::IdRole).toInt(), false)));
}

void LayerList::mergeSelected()
{
	QModelIndex index = currentSelection();
	if(!index.isValid())
		return;

	emit layerCommand(protocol::MessagePtr(new protocol::UndoPoint(0)));
	emit layerCommand(protocol::MessagePtr(new protocol::LayerDelete(0, index.data(canvas::LayerListModel::IdRole).toInt(), true)));
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
 */
void LayerList::onLayerCreate(const QModelIndex&, int, int)
{
	// Automatically select the first layer
	if(m_canvas->layerlist()->rowCount()==1)
		_ui->layerlist->selectionModel()->select(_ui->layerlist->model()->index(0,0), QItemSelectionModel::SelectCurrent);
}

/**
 * @brief Respond to layer deletion
 */
void LayerList::onLayerDelete(const QModelIndex &, int first, int last)
{
	const QModelIndex cursel = currentSelection();
	int row = cursel.isValid() ? 0 : cursel.row();

	// Automatically select neighbouring on deletion
	if(row >= first && row <= last) {
		if(first==0)
			row=last+1;
		else
			row = first-1;
		selectLayer(m_canvas->layerlist()->index(row).data(canvas::LayerListModel::IdRole).toInt());
	}
}

void LayerList::onLayerReorder()
{
	if(m_selectedId)
		selectLayer(m_selectedId);
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
	return m_selectedId;
}

bool LayerList::isCurrentLayerLocked() const
{
	if(!m_canvas)
		return false;

	QModelIndexList idx = _ui->layerlist->selectionModel()->selectedIndexes();
	if(!idx.isEmpty()) {
		const paintcore::LayerInfo &item = idx.at(0).data().value<paintcore::LayerInfo>();
		return item.hidden || item.isLockedFor(m_canvas->localUserId());
	}
	return false;
}

void LayerList::selectionChanged(const QItemSelection &selected)
{
	bool on = selected.count() > 0;

	if(on) {
		QModelIndex cs = currentSelection();
		dataChanged(cs,cs);
		m_selectedId = cs.data(canvas::LayerListModel::IdRole).toInt();
	} else {
		m_selectedId = 0;
	}

	updateLockedControls();

	emit layerSelected(m_selectedId);
}

void LayerList::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
	const int myRow = currentSelection().row();
	if(topLeft.row() <= myRow && myRow <= bottomRight.row()) {
		const paintcore::LayerInfo &layer = currentSelection().data().value<paintcore::LayerInfo>();
		_noupdate = true;
		_menuHideAction->setChecked(layer.hidden);
		_ui->opacity->setValue(layer.opacity);

		int blendmode = _ui->blendmode->currentData().toInt();
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
		// TODO use change flags to detect if this really changed
		emit activeLayerVisibilityChanged();
		_noupdate = false;
	}
}

}
