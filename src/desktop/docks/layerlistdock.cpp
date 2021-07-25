/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2019 Calle Laakkonen

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
#include "canvas/userlist.h"
#include "docks/layerlistdock.h"
#include "docks/layerlistdelegate.h"
#include "docks/layeraclmenu.h"
#include "docks/utils.h"
#include "core/blendmodes.h"
#include "utils/changeflags.h"

#include "../libshared/net/layer.h"
#include "../libshared/net/meta2.h"
#include "../libshared/net/undo.h"

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
	: QDockWidget(tr("Layers"), parent), m_canvas(nullptr), m_selectedId(0), m_noupdate(false),
	m_addLayerAction(nullptr), m_duplicateLayerAction(nullptr), m_mergeLayerAction(nullptr), m_deleteLayerAction(nullptr)
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

	m_layerProperties = new dialogs::LayerProperties(this);
	connect(m_layerProperties, &dialogs::LayerProperties::propertiesChanged,
			this, &LayerList::emitPropertyChangeCommands);

	// Layer menu

	m_layermenu = new QMenu(this);
	m_menuInsertAction = m_layermenu->addAction(tr("Insert layer"), this, SLOT(insertLayer()));

	m_menuSeparator = m_layermenu->addSeparator();

	m_menuHideAction = m_layermenu->addAction(tr("Hide from self"), this, SLOT(hideSelected()));
	m_menuHideAction->setCheckable(true);

	m_menuFixedAction = m_layermenu->addAction(tr("Fixed"), this, SLOT(setSelectedFixed(bool)));
	m_menuFixedAction->setCheckable(true);

	QActionGroup *makeDefault = new QActionGroup(this);
	makeDefault->setExclusive(true);
	m_menuDefaultAction = m_layermenu->addAction(tr("Default"), this, SLOT(setSelectedDefault()));
	m_menuDefaultAction->setCheckable(true);
	m_menuDefaultAction->setActionGroup(makeDefault);

	m_menuPropertiesAction = m_layermenu->addAction(tr("Properties..."),
			this, &LayerList::showPropertiesOfSelected);

	// Layer ACL menu
	m_aclmenu = new LayerAclMenu(this);
	m_ui->lockButton->setMenu(m_aclmenu);

	connect(m_ui->layerlist, &QListView::customContextMenuRequested, this, &LayerList::layerContextMenu);

	connect(m_ui->opacity, SIGNAL(valueChanged(int)), this, SLOT(opacityAdjusted()));
	connect(m_ui->blendmode, SIGNAL(currentIndexChanged(int)), this, SLOT(blendModeChanged()));
	connect(m_aclmenu, &LayerAclMenu::layerAclChange, this, &LayerList::changeLayerAcl);
	connect(m_aclmenu, &LayerAclMenu::layerCensoredChange, this, &LayerList::censorSelected);

	selectionChanged(QItemSelection());

	// The opacity update timer is used to limit the rate of layer
	// update messages sent over the network when the user drags or scrolls
	// on the opacity slider.
	m_opacityUpdateTimer = new QTimer(this);
	m_opacityUpdateTimer->setSingleShot(true);
	connect(m_opacityUpdateTimer, &QTimer::timeout, this, &LayerList::sendOpacityUpdate);

	// Custom layer list item delegate
	LayerListDelegate *del = new LayerListDelegate(this);
	connect(del, &LayerListDelegate::toggleVisibility, this, &LayerList::setLayerVisibility);
	connect(del, &LayerListDelegate::editProperties, this, &LayerList::showPropertiesOfIndex);
	m_ui->layerlist->setItemDelegate(del);
}

LayerList::~LayerList()
{
	delete m_ui;
}

void LayerList::setCanvas(canvas::CanvasModel *canvas)
{
	m_canvas = canvas;
	m_ui->layerlist->setModel(canvas->layerlist());

	m_aclmenu->setUserList(canvas->userlist()->onlineUsers());

	connect(canvas->layerlist(), &canvas::LayerListModel::rowsInserted, this, &LayerList::onLayerCreate);
	connect(canvas->layerlist(), &canvas::LayerListModel::rowsAboutToBeRemoved, this, &LayerList::beforeLayerDelete);
	connect(canvas->layerlist(), &canvas::LayerListModel::rowsRemoved, this, &LayerList::onLayerDelete);
	connect(canvas->layerlist(), &canvas::LayerListModel::layersReordered, this, &LayerList::onLayerReorder);
	connect(canvas->layerlist(), &canvas::LayerListModel::modelReset, this, &LayerList::onLayerReorder);
	connect(canvas->layerlist(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(dataChanged(QModelIndex,QModelIndex)));
	connect(canvas->aclFilter(), &canvas::AclFilter::featureAccessChanged, this, &LayerList::onFeatureAccessChange);
	connect(canvas->aclFilter(), &canvas::AclFilter::layerAclChanged, this, &LayerList::lockStatusChanged);
	connect(m_ui->layerlist->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(selectionChanged(QItemSelection)));

	// Init
	m_ui->layerlist->setEnabled(true);
	updateLockedControls();
}

void LayerList::setLayerEditActions(QAction *add, QAction *duplicate, QAction *merge, QAction *del)
{
	Q_ASSERT(add);
	Q_ASSERT(duplicate);
	Q_ASSERT(merge);
	Q_ASSERT(del);
	m_addLayerAction = add;
	m_duplicateLayerAction = duplicate;
	m_mergeLayerAction = merge;
	m_deleteLayerAction = del;

	connect(m_addLayerAction, &QAction::triggered, this, &LayerList::addLayer);
	connect(m_duplicateLayerAction, &QAction::triggered, this, &LayerList::duplicateLayer);
	connect(m_mergeLayerAction, &QAction::triggered, this, &LayerList::mergeSelected);
	connect(m_deleteLayerAction, &QAction::triggered, this, &LayerList::deleteSelected);

	// Insert some actions to the context menu too
	m_layermenu->insertAction(m_menuSeparator, duplicate);
	m_layermenu->insertAction(m_menuSeparator, del);
	m_layermenu->insertAction(m_menuSeparator, merge);

	updateLockedControls();
}

void LayerList::onFeatureAccessChange(canvas::Feature feature, bool canUse)
{
	Q_UNUSED(canUse);
	switch(feature) {
		case canvas::Feature::EditLayers:
		case canvas::Feature::OwnLayers:
			updateLockedControls();
		default: break;
	}
}

void LayerList::updateLockedControls()
{
	// The basic permissions
	const bool canEdit = m_canvas && m_canvas->aclFilter()->canUseFeature(canvas::Feature::EditLayers);
	const bool ownLayers = m_canvas && m_canvas->aclFilter()->canUseFeature(canvas::Feature::OwnLayers);

	// Layer creation actions work as long as we have an editing permission
	const bool canAdd = canEdit | ownLayers;
	const bool hasEditActions = m_addLayerAction != nullptr;
	if(hasEditActions) {
		m_addLayerAction->setEnabled(canAdd);
		m_menuInsertAction->setEnabled(canAdd);
	}

	// Rest of the controls need a selection to work.
	const bool enabled = m_selectedId && (canEdit || (ownLayers && (m_selectedId>>8) == m_canvas->localUserId()));

	m_ui->lockButton->setEnabled(enabled || (m_canvas && !m_canvas->isOnline())); // layer lock is available in offline mode
	if(hasEditActions) {
		m_duplicateLayerAction->setEnabled(enabled);
		m_deleteLayerAction->setEnabled(enabled);
		m_mergeLayerAction->setEnabled(enabled && canMergeCurrent());
	}
	m_ui->opacity->setEnabled(enabled);
	m_ui->blendmode->setEnabled(enabled);

	m_ui->layerlist->setEditTriggers(enabled ? QAbstractItemView::DoubleClicked : QAbstractItemView::NoEditTriggers);
	m_menuPropertiesAction->setEnabled(enabled);
	m_menuDefaultAction->setEnabled(enabled);
	m_menuFixedAction->setEnabled(enabled);
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
	selectLayerIndex(m_canvas->layerlist()->layerIndex(id));
}

void LayerList::selectLayerIndex(QModelIndex index, bool scrollTo)
{
	if(index.isValid()) {
		m_ui->layerlist->selectionModel()->select(
			index, QItemSelectionModel::SelectCurrent|QItemSelectionModel::Clear);
		if(scrollTo) {
			m_ui->layerlist->scrollTo(index);
		}
	}
}

void LayerList::selectAbove()
{
	QModelIndex current = currentSelection();
	selectLayerIndex(current.sibling(current.row() - 1, 0), true);
}

void LayerList::selectBelow()
{
	QModelIndex current = currentSelection();
	selectLayerIndex(current.sibling(current.row() + 1, 0), true);
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

static protocol::MessagePtr updateLayerAttributesMessage(uint8_t contextId, const canvas::LayerListItem &layer, ChangeFlags<uint8_t> flagChanges, int opacity, int blend)
{
	return protocol::MessagePtr(new protocol::LayerAttributes(
		contextId,
		layer.id,
		0,
		flagChanges.update(layer.attributeFlags()),
		opacity>=0 ? opacity : layer.opacity*255,
		blend >= 0 ? blend : int(layer.blend)
	));
}

void LayerList::sendOpacityUpdate()
{
	QModelIndex index = currentSelection();
	if(index.isValid()) {
		canvas::LayerListItem layer = index.data().value<canvas::LayerListItem>();
		emit layerCommand(updateLayerAttributesMessage(
			m_canvas->localUserId(),
			index.data().value<canvas::LayerListItem>(),
			ChangeFlags<uint8_t>{},
			m_ui->opacity->value(),
			-1
		));
	}
}

void LayerList::blendModeChanged()
{
	// Avoid infinite loop
	if(m_noupdate)
		return;

	QModelIndex index = currentSelection();
	if(index.isValid()) {
		emit layerCommand(updateLayerAttributesMessage(
			m_canvas->localUserId(),
			index.data().value<canvas::LayerListItem>(),
			ChangeFlags<uint8_t>{},
			-1,
			m_ui->blendmode->currentData().toInt()
		));
	}
}

void LayerList::censorSelected(bool censor)
{
	QModelIndex index = currentSelection();
	if(index.isValid()) {
		emit layerCommand(updateLayerAttributesMessage(
			m_canvas->localUserId(),
			index.data().value<canvas::LayerListItem>(),
			ChangeFlags<uint8_t>().set(protocol::LayerAttributes::FLAG_CENSOR, censor),
			-1,
			-1
		));
	}
}

void LayerList::setSelectedFixed(bool fixed)
{
	QModelIndex index = currentSelection();
	if(index.isValid()) {
		emit layerCommand(updateLayerAttributesMessage(
			m_canvas->localUserId(),
			index.data().value<canvas::LayerListItem>(),
			ChangeFlags<uint8_t>().set(protocol::LayerAttributes::FLAG_FIXED, fixed),
			-1,
			-1
		));
	}
}

void LayerList::hideSelected()
{
	QModelIndex index = currentSelection();
	if(index.isValid())
		setLayerVisibility(index.data().value<canvas::LayerListItem>().id, !m_menuHideAction->isChecked());
}

void LayerList::setLayerVisibility(int layerId, bool visible)
{
	emit layerCommand(protocol::MessagePtr(new protocol::LayerVisibility(m_canvas->localUserId(), layerId, visible)));
}

void LayerList::changeLayerAcl(bool lock, canvas::Tier tier, QList<uint8_t> exclusive)
{
	const QModelIndex index = currentSelection();
	if(index.isValid()) {
		const int layerId = index.data(canvas::LayerListModel::IdRole).toInt();
		emit layerCommand(protocol::MessagePtr(new protocol::LayerACL(
			m_canvas->localUserId(),
			layerId,
			lock,
			int(tier),
			exclusive
		)));
	}
}

void LayerList::showLayerNumbers(bool show)
{
	LayerListDelegate *del = qobject_cast<LayerListDelegate*>(m_ui->layerlist->itemDelegate());
	Q_ASSERT(del);
	del->setShowNumbers(show);
}

/**
 * @brief Layer add button pressed
 */
void LayerList::addLayer()
{
	const canvas::LayerListModel *layers = qobject_cast<canvas::LayerListModel*>(m_ui->layerlist->model());
	Q_ASSERT(layers);

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

	const canvas::LayerListModel *layers = qobject_cast<canvas::LayerListModel*>(m_ui->layerlist->model());
	Q_ASSERT(layers);

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

	const canvas::LayerListModel *layers = qobject_cast<canvas::LayerListModel*>(m_ui->layerlist->model());
	Q_ASSERT(layers);

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
		   !m_canvas->aclFilter()->isLayerLocked(below.data(canvas::LayerListModel::IdRole).toInt())
			;
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

void LayerList::showPropertiesOfSelected()
{
	showPropertiesOfIndex(currentSelection());
}

void LayerList::showPropertiesOfIndex(QModelIndex index)
{
	if(index.isValid()) {
		const canvas::LayerListItem &item = index.data().value<canvas::LayerListItem>();
		m_layerProperties->setLayerData(dialogs::LayerProperties::LayerData {
			item.id,
			item.title,
			item.opacity,
			item.blend,
			item.hidden,
			item.fixed,
			index.data(canvas::LayerListModel::IsDefaultRole).toBool(),
		});
		m_layerProperties->show();
	}
}

/**
 * @brief Respond to creation of a new layer
 */
void LayerList::onLayerCreate(const QModelIndex&, int, int)
{
	// Automatically select the first layer
	if(m_canvas->layerlist()->rowCount()==1)
		m_ui->layerlist->selectionModel()->select(m_ui->layerlist->model()->index(0,0), QItemSelectionModel::SelectCurrent);
	else // remind ourselves of the current selection
		emit layerSelected(m_selectedId);
}

void LayerList::beforeLayerDelete()
{
	const QModelIndex cursel = currentSelection();
	m_lastSelectedRow = cursel.isValid() ? cursel.row() : 0;
}
/**
 * @brief Respond to layer deletion
 */
void LayerList::onLayerDelete(const QModelIndex &, int first, int last)
{
	int row = m_lastSelectedRow;

	if(m_canvas->layerlist()->rowCount() == 0)
		return;

	// Automatically select neighbouring on deletion
	if(row >= first && row <= last) {
		row = qBound(0, row, m_canvas->layerlist()->rowCount()-1);
		selectLayerIndex(m_canvas->layerlist()->index(row), true);
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

bool LayerList::isCurrentLayerLocked() const
{
	if(!m_canvas)
		return false;

	QModelIndex idx = currentSelection();
	if(idx.isValid()) {
		const canvas::LayerListItem &item = idx.data().value<canvas::LayerListItem>();
		return item.hidden
			|| m_canvas->aclFilter()->isLayerLocked(item.id)
			|| (m_canvas->layerStack()->isCensored() && item.censored);
	}
	return false;
}

void LayerList::selectionChanged(const QItemSelection &selected)
{
	bool on = selected.count() > 0;

	if(on) {
		QModelIndex cs = currentSelection();
		m_selectedId = cs.data(canvas::LayerListModel::IdRole).toInt();
		dataChanged(cs,cs);
	} else {
		m_selectedId = 0;
	}

	updateLockedControls();

	emit layerSelected(m_selectedId);
}

void LayerList::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
	// Refresh UI when seleceted layer's data changes
	const int myRow = currentSelection().row();
	if(topLeft.row() <= myRow && myRow <= bottomRight.row()) {
		const canvas::LayerListItem &layer = currentSelection().data().value<canvas::LayerListItem>();
		m_noupdate = true;
		m_menuHideAction->setChecked(layer.hidden);
		m_aclmenu->setCensored(layer.censored);
		m_menuDefaultAction->setChecked(currentSelection().data(canvas::LayerListModel::IsDefaultRole).toBool());
		m_menuFixedAction->setChecked(layer.fixed);
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

		lockStatusChanged(layer.id);
		updateLockedControls();

		// TODO use change flags to detect if this really changed
		emit activeLayerVisibilityChanged();
		m_noupdate = false;
	}
}

void LayerList::lockStatusChanged(int layerId)
{
	if(m_selectedId == layerId) {
		const canvas::AclFilter::LayerAcl acl = m_canvas->aclFilter()->layerAcl(layerId);
		m_ui->lockButton->setChecked(acl.locked || acl.tier != canvas::Tier::Guest || !acl.exclusive.isEmpty());
		m_aclmenu->setAcl(acl.locked, acl.tier, acl.exclusive);
	}
}

void LayerList::emitPropertyChangeCommands(const dialogs::LayerProperties::ChangedLayerData &c)
{
	// Changes that get grouped together into a single layer attribute change message.
	static const unsigned int CHANGE_LAYER_ATTRIBUTES =
			dialogs::LayerProperties::CHANGE_OPACITY |
			dialogs::LayerProperties::CHANGE_BLEND |
			dialogs::LayerProperties::CHANGE_FIXED;

	uint16_t layerId = c.id;
	QModelIndex layerIndex = m_canvas->layerlist()->layerIndex(layerId);
	if(!m_canvas || !layerIndex.isValid()) {
		return; // Looks like the layer was deleted from under us.
	}

	uint8_t contextId = m_canvas->localUserId();
	if(c.changes & dialogs::LayerProperties::CHANGE_TITLE) {
		emit layerCommand(protocol::MessagePtr(
				new protocol::LayerRetitle(contextId, layerId, c.title)));
	}

	if(c.changes & CHANGE_LAYER_ATTRIBUTES) {
		canvas::LayerListItem layer = layerIndex.data().value<canvas::LayerListItem>();

		ChangeFlags<uint8_t> flags;
		if(c.changes & dialogs::LayerProperties::CHANGE_FIXED) {
			flags.set(protocol::LayerAttributes::FLAG_FIXED, c.fixed);
		}

		int opacity = -1;
		if(c.changes & dialogs::LayerProperties::CHANGE_OPACITY) {
			opacity = c.opacity * 255;
		}

		int blend = -1;
		if(c.changes & dialogs::LayerProperties::CHANGE_BLEND) {
			blend = c.blend;
		}

		emit layerCommand(updateLayerAttributesMessage(
				contextId, layer, flags, opacity, blend));
	}

	if((c.changes & dialogs::LayerProperties::CHANGE_DEFAULT) && c.defaultLayer) {
		emit layerCommand(protocol::MessagePtr(
				new protocol::DefaultLayer(contextId, layerId)));
	}
}

}
