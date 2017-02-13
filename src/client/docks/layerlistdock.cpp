/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2017 Calle Laakkonen

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
	: QDockWidget(tr("Layers"), parent), m_canvas(nullptr), m_selectedId(0), m_noupdate(false), m_op(false), m_lockctrl(false), m_ownlayers(false)
{
	m_ui = new Ui_LayerBox;
	QWidget *w = new QWidget(this);
	setWidget(w);
	m_ui->setupUi(w);

	setStyleSheet(defaultDockStylesheet());

	m_ui->layerlist->setDragEnabled(true);
	m_ui->layerlist->viewport()->setAcceptDrops(true);
	m_ui->layerlist->setEnabled(false);
	m_ui->layerlist->setSelectionMode(QAbstractItemView::SingleSelection);

	// Populate blend mode combobox
	for(auto bm : paintcore::getBlendModeNames(paintcore::BlendMode::LayerMode))
		m_ui->blendmode->addItem(bm.second, bm.first);

	// Layer menu

	m_layermenu = new QMenu(this);
	m_menuInsertAction = m_layermenu->addAction(tr("Insert layer"), this, &LayerList::insertLayer);

	m_layermenu->addSeparator();

	m_menuHideAction = m_layermenu->addAction(tr("Hide from self"), this, &LayerList::hideSelected);
	m_menuHideAction->setCheckable(true);

	QActionGroup *makeDefault = new QActionGroup(this);
	makeDefault->setExclusive(true);
	m_menuDefaultAction = m_layermenu->addAction(tr("Default"), this, &LayerList::setSelectedDefault);
	m_menuDefaultAction->setCheckable(true);
	m_menuDefaultAction->setActionGroup(makeDefault);

	m_menuRenameAction = m_layermenu->addAction(tr("Rename"), this, &LayerList::renameSelected);
	m_menuMergeAction = m_layermenu->addAction(tr("Merge down"), this, &LayerList::mergeSelected);
	m_menuDeleteAction = m_layermenu->addAction(tr("Delete"), this, &LayerList::deleteSelected);

	// Layer ACL menu
	m_aclmenu = new LayerAclMenu(this);
	m_ui->lockButton->setMenu(m_aclmenu);

	connect(m_ui->layerlist, &QListView::customContextMenuRequested, this, &LayerList::layerContextMenu);

	// Layer edit menu (hamburger button)
	QMenu *boxmenu = new QMenu(this);
	m_addLayerAction = boxmenu->addAction(tr("New"), this, &LayerList::addLayer);
	m_duplicateLayerAction = boxmenu->addAction(tr("Duplicate"), this, &LayerList::duplicateLayer);
	m_deleteLayerAction = boxmenu->addAction(tr("Delete"), this, &LayerList::deleteOrMergeSelected);

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
	m_viewMode = boxmenu->addMenu(QString()); // title is set later
	m_viewMode->addActions(viewmodes->actions());

	m_showNumbersAction = boxmenu->addAction(tr("Show numbers"));
	m_showNumbersAction->setCheckable(true);

	m_ui->menuButton->setMenu(boxmenu);

	connect(m_ui->opacity, SIGNAL(valueChanged(int)), this, SLOT(opacityAdjusted()));
	connect(m_ui->blendmode, SIGNAL(currentIndexChanged(int)), this, SLOT(blendModeChanged()));
	connect(m_aclmenu, SIGNAL(layerAclChange(bool, QList<uint8_t>)), this, SLOT(changeLayerAcl(bool, QList<uint8_t>)));
	connect(m_viewMode, SIGNAL(triggered(QAction*)), this, SLOT(layerViewModeTriggered(QAction*)));
	connect(m_showNumbersAction, &QAction::triggered, this, &LayerList::showLayerNumbers);

	selectionChanged(QItemSelection());

	// The opacity update timer is used to limit the rate of layer
	// update messages sent over the network when the user drags or scrolls
	// on the opacity slider.
	m_opacityUpdateTimer = new QTimer(this);
	m_opacityUpdateTimer->setSingleShot(true);
	connect(m_opacityUpdateTimer, &QTimer::timeout, this, &LayerList::sendOpacityUpdate);
}

LayerList::~LayerList()
{
	delete m_ui;
}

void LayerList::setCanvas(canvas::CanvasModel *canvas)
{
	m_canvas = canvas;
	m_ui->layerlist->setModel(canvas->layerlist());

	LayerListDelegate *del = new LayerListDelegate(this);
	connect(del, &LayerListDelegate::layerCommand, [this](protocol::MessagePtr msg) {
		msg->setContextId(m_canvas->localUserId());
		emit layerCommand(msg);
	});
	connect(del, &LayerListDelegate::toggleVisibility, this, &LayerList::setLayerVisibility);
	m_ui->layerlist->setItemDelegate(del);

	m_aclmenu->setUserList(canvas->userlist());

	connect(canvas->layerlist(), &canvas::LayerListModel::rowsInserted, this, &LayerList::onLayerCreate);
	connect(canvas->layerlist(), &canvas::LayerListModel::rowsAboutToBeRemoved, this, &LayerList::onLayerDelete);
	connect(canvas->layerlist(), SIGNAL(layersReordered()), this, SLOT(onLayerReorder()));
	connect(canvas->layerlist(), SIGNAL(modelReset()), this, SLOT(onLayerReorder()));
	connect(canvas->layerlist(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(dataChanged(QModelIndex,QModelIndex)));
	connect(canvas->aclFilter(), &canvas::AclFilter::layerControlLockChanged, this, &LayerList::setControlsLocked);
	connect(canvas->aclFilter(), &canvas::AclFilter::ownLayersChanged, this, &LayerList::setOwnLayers);
	connect(m_ui->layerlist->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(selectionChanged(QItemSelection)));

	// Restore settings
	QSettings cfg;
	m_showNumbersAction->setChecked(cfg.value("setting/layernumbers", false).toBool());
	del->setShowNumbers(m_showNumbersAction->isChecked());

	// Init
	m_ui->layerlist->setEnabled(true);
	m_viewMode->actions()[0]->setChecked(true);
	layerViewModeTriggered(m_viewMode->actions()[0]);
	setControlsLocked(false);
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

	m_addLayerAction->setEnabled(m_canvas && m_canvas->aclFilter()->canCreateLayer());
	m_menuInsertAction->setEnabled(enabled);

	// Rest of the controls need a selection to work.
	// If there is a selection, but the layer is locked, the controls
	// are locked for non-operators.
	if(m_selectedId)
		enabled = enabled & (m_op | !isCurrentLayerLocked());
	else
		enabled = false;

	m_ui->lockButton->setEnabled(
		m_op ||
		(m_canvas && !m_canvas->isOnline()) ||
		(m_ownlayers && m_canvas && (currentLayer()>>8) == m_canvas->localUserId())
	);
	m_duplicateLayerAction->setEnabled(enabled);
	m_deleteLayerAction->setEnabled(enabled);
	m_ui->opacity->setEnabled(enabled);
	m_ui->blendmode->setEnabled(enabled);

	m_ui->layerlist->setEditTriggers(enabled ? QAbstractItemView::DoubleClicked : QAbstractItemView::NoEditTriggers);
	m_menuDeleteAction->setEnabled(enabled);
	m_menuMergeAction->setEnabled(enabled && canMergeCurrent());
	m_menuRenameAction->setEnabled(enabled);
	m_menuDefaultAction->setEnabled(enabled && m_canvas->isOnline());
}

void LayerList::layerContextMenu(const QPoint &pos)
{
	QModelIndex index = m_ui->layerlist->indexAt(pos);
	if(index.isValid()) {
		m_layermenu->popup(m_ui->layerlist->mapToGlobal(pos));
	}
}

void LayerList::selectLayer(int id)
{
	m_ui->layerlist->selectionModel()->select(m_canvas->layerlist()->layerIndex(id), QItemSelectionModel::SelectCurrent|QItemSelectionModel::Clear);
}

void LayerList::selectAbove()
{
	QModelIndex current = currentSelection();
	QModelIndex prev = current.sibling(current.row() - 1, 0);
	if(prev.isValid())
		m_ui->layerlist->selectionModel()->select(prev, QItemSelectionModel::SelectCurrent|QItemSelectionModel::Clear);
}

void LayerList::selectBelow()
{
	QModelIndex current = currentSelection();
	QModelIndex prev = current.sibling(current.row() + 1, 0);
	if(prev.isValid())
		m_ui->layerlist->selectionModel()->select(prev, QItemSelectionModel::SelectCurrent|QItemSelectionModel::Clear);
}

void LayerList::opacityAdjusted()
{
	// Avoid infinite loop
	if(m_noupdate)
		return;

	QModelIndex index = currentSelection();
	if(index.isValid()) {
		canvas::LayerListItem layer = index.data().value<canvas::LayerListItem>();
		float opacity = m_ui->opacity->value() / 255.0;

		m_canvas->layerlist()->previewOpacityChange(layer.id, opacity);
		m_opacityUpdateTimer->start(100);
	}
}

void LayerList::sendOpacityUpdate()
{
	QModelIndex index = currentSelection();
	if(index.isValid()) {
		canvas::LayerListItem layer = index.data().value<canvas::LayerListItem>();
		emit layerCommand(protocol::MessagePtr(new protocol::LayerAttributes(m_canvas->localUserId(), layer.id, m_ui->opacity->value(), int(layer.blend))));
	}
}

void LayerList::blendModeChanged()
{
	// Avoid infinite loop
	if(m_noupdate)
		return;

	QModelIndex index = currentSelection();
	if(index.isValid()) {
		canvas::LayerListItem layer = index.data().value<canvas::LayerListItem>();
		emit layerCommand(protocol::MessagePtr(new protocol::LayerAttributes(m_canvas->localUserId(), layer.id, layer.opacity*255, m_ui->blendmode->currentData().toInt())));
	}
}

void LayerList::hideSelected()
{
	QModelIndex index = currentSelection();
	if(index.isValid())
		setLayerVisibility(index.data().value<canvas::LayerListItem>().id, m_menuHideAction->isChecked());
}

void LayerList::setLayerVisibility(int layerId, bool visible)
{
	emit layerCommand(protocol::MessagePtr(new protocol::LayerVisibility(m_canvas->localUserId(), layerId, visible)));
}

void LayerList::changeLayerAcl(bool lock, QList<uint8_t> exclusive)
{
	QModelIndex index = currentSelection();
	if(index.isValid()) {
		canvas::LayerListItem layer = index.data().value<canvas::LayerListItem>();
		layer.locked = lock;
		layer.exclusive = exclusive;
		emit layerCommand(protocol::MessagePtr(new protocol::LayerACL(m_canvas->localUserId(), layer.id, layer.locked, layer.exclusive)));
	}
}

void LayerList::layerViewModeTriggered(QAction *action)
{
	m_viewMode->setTitle(tr("Mode:") + " " + action->text());
	emit layerViewModeSelected(action->property("viewmode").toInt());
}

void LayerList::showLayerNumbers(bool show)
{
	LayerListDelegate *del = static_cast<LayerListDelegate*>(m_ui->layerlist->itemDelegate());
	del->setShowNumbers(show);

	QSettings cfg;
	cfg.setValue("setting/layernumbers", m_showNumbersAction->isChecked());

}

/**
 * @brief Layer add button pressed
 */
void LayerList::addLayer()
{
	const canvas::LayerListModel *layers = static_cast<canvas::LayerListModel*>(m_ui->layerlist->model());

	const int id = layers->getAvailableLayerId();
	if(id==0)
		return;

	const QString name = layers->getAvailableLayerName(tr("Layer"));

	emit layerCommand(protocol::MessagePtr(new protocol::UndoPoint(m_canvas->localUserId())));
	emit layerCommand(protocol::MessagePtr(new protocol::LayerCreate(m_canvas->localUserId(), id, 0, 0, 0, name)));
}

/**
 * @brief Insert a new layer above the current selection
 */
void LayerList::insertLayer()
{
	const QModelIndex index = currentSelection();
	const canvas::LayerListItem layer = index.data().value<canvas::LayerListItem>();

	const canvas::LayerListModel *layers = static_cast<canvas::LayerListModel*>(m_ui->layerlist->model());

	const int id = layers->getAvailableLayerId();
	if(id==0)
		return;

	const QString name = layers->getAvailableLayerName(tr("Layer"));

	emit layerCommand(protocol::MessagePtr(new protocol::UndoPoint(m_canvas->localUserId())));
	emit layerCommand(protocol::MessagePtr(new protocol::LayerCreate(m_canvas->localUserId(), id, layer.id, 0, protocol::LayerCreate::FLAG_INSERT, name)));
}

void LayerList::duplicateLayer()
{
	const QModelIndex index = currentSelection();
	const canvas::LayerListItem layer = index.data().value<canvas::LayerListItem>();

	const canvas::LayerListModel *layers = static_cast<canvas::LayerListModel*>(m_ui->layerlist->model());

	const int id = layers->getAvailableLayerId();
	if(id==0)
		return;

	const QString name = layers->getAvailableLayerName(layer.title);

	emit layerCommand(protocol::MessagePtr(new protocol::UndoPoint(m_canvas->localUserId())));
	emit layerCommand(protocol::MessagePtr(new protocol::LayerCreate(m_canvas->localUserId(), id, layer.id, 0, protocol::LayerCreate::FLAG_INSERT | protocol::LayerCreate::FLAG_COPY, name)));
}

bool LayerList::canMergeCurrent() const
{
	const QModelIndex index = currentSelection();
	const QModelIndex below = index.sibling(index.row()+1, 0);

	return index.isValid() && below.isValid() &&
		   !below.data().value<canvas::LayerListItem>().isLockedFor(m_canvas->localUserId());
}

void LayerList::deleteOrMergeSelected()
{
	QModelIndex index = currentSelection();
	if(!index.isValid())
		return;

	canvas::LayerListItem layer = index.data().value<canvas::LayerListItem>();

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

	emit layerCommand(protocol::MessagePtr(new protocol::UndoPoint(m_canvas->localUserId())));
	emit layerCommand(protocol::MessagePtr(new protocol::LayerDelete(m_canvas->localUserId(), index.data().value<canvas::LayerListItem>().id, false)));
}

void LayerList::setSelectedDefault()
{
	QModelIndex index = currentSelection();
	if(!index.isValid())
		return;

	emit layerCommand(protocol::MessagePtr(new protocol::DefaultLayer(m_canvas->localUserId(), index.data(canvas::LayerListModel::IdRole).toInt())));
}

void LayerList::mergeSelected()
{
	QModelIndex index = currentSelection();
	if(!index.isValid())
		return;

	emit layerCommand(protocol::MessagePtr(new protocol::UndoPoint(m_canvas->localUserId())));
	emit layerCommand(protocol::MessagePtr(new protocol::LayerDelete(m_canvas->localUserId(), index.data().value<canvas::LayerListItem>().id, true)));
}

void LayerList::renameSelected()
{
	QModelIndex index = currentSelection();
	if(!index.isValid())
		return;

	m_ui->layerlist->edit(index);
}

/**
 * @brief Respond to creation of a new layer
 */
void LayerList::onLayerCreate(const QModelIndex&, int, int)
{
	// Automatically select the first layer
	if(m_canvas->layerlist()->rowCount()==1)
		m_ui->layerlist->selectionModel()->select(m_ui->layerlist->model()->index(0,0), QItemSelectionModel::SelectCurrent);
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
	QModelIndexList sel = m_ui->layerlist->selectionModel()->selectedIndexes();
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

	QModelIndexList idx = m_ui->layerlist->selectionModel()->selectedIndexes();
	if(!idx.isEmpty()) {
		const canvas::LayerListItem &item = idx.at(0).data().value<canvas::LayerListItem>();
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
		const canvas::LayerListItem &layer = currentSelection().data().value<canvas::LayerListItem>();
		m_noupdate = true;
		m_menuHideAction->setChecked(layer.hidden);
		m_menuDefaultAction->setChecked(currentSelection().data(canvas::LayerListModel::IsDefaultRole).toBool());
		m_ui->opacity->setValue(layer.opacity * 255);

		int blendmode = m_ui->blendmode->currentData().toInt();
		if(blendmode != layer.blend) {
			for(int i=0;i<m_ui->blendmode->count();++i) {
				if(m_ui->blendmode->itemData(i).toInt() == layer.blend) {
				m_ui->blendmode->setCurrentIndex(i);
				break;
				}
			}
		}

		m_ui->lockButton->setChecked(layer.locked || !layer.exclusive.isEmpty());
		m_aclmenu->setAcl(layer.locked, layer.exclusive);
		updateLockedControls();
		// TODO use change flags to detect if this really changed
		emit activeLayerVisibilityChanged();
		m_noupdate = false;
	}
}

}
