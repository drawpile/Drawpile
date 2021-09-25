/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2021 Calle Laakkonen

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
#include "canvas/canvasmodel.h"
#include "canvas/blendmodes.h"
#include "canvas/userlist.h"
#include "canvas/paintengine.h"
#include "docks/layerlistdock.h"
#include "docks/layerlistdelegate.h"
#include "docks/layeraclmenu.h"
#include "docks/utils.h"
#include "utils/changeflags.h"
#include "net/envelopebuilder.h"

#include "../rustpile/rustpile.h"

#include "ui_layerbox.h"

#include <QDebug>
#include <QItemSelection>
#include <QMessageBox>
#include <QPushButton>
#include <QActionGroup>
#include <QTimer>
#include <QSettings>
#include <QStandardItemModel>
#include <QScrollBar>

namespace docks {

LayerList::LayerList(QWidget *parent)
	: QDockWidget(tr("Layers"), parent),
	  m_canvas(nullptr), m_selectedId(0), m_nearestToDeletedId(0),
	  m_noupdate(false),
	  m_addLayerAction(nullptr), m_duplicateLayerAction(nullptr),
	  m_mergeLayerAction(nullptr), m_deleteLayerAction(nullptr)
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
	for(const auto &bm : canvas::blendmode::layerModeNames())
		m_ui->blendmode->addItem(bm.second, int(bm.first));
	m_ui->blendmode->addItem(tr("Pass-through"), -1);

	m_layerProperties = new dialogs::LayerProperties(this);
	connect(m_layerProperties, &dialogs::LayerProperties::propertiesChanged,
			this, &LayerList::emitPropertyChangeCommands);

	// Layer menu

	m_layermenu = new QMenu(this);
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

	connect(m_ui->layerlist, &QTreeView::customContextMenuRequested, this, &LayerList::layerContextMenu);

	connect(m_ui->opacity, &QSlider::valueChanged, this, &LayerList::opacityAdjusted);
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

	connect(canvas->layerlist(), &canvas::LayerListModel::modelAboutToBeReset, this, &LayerList::beforeLayerReset);
	connect(canvas->layerlist(), &canvas::LayerListModel::modelReset, this, &LayerList::afterLayerReset);

	connect(canvas->aclState(), &canvas::AclState::featureAccessChanged, this, &LayerList::onFeatureAccessChange);
	connect(canvas->aclState(), &canvas::AclState::layerAclChanged, this, &LayerList::lockStatusChanged);
	connect(m_ui->layerlist->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(selectionChanged(QItemSelection)));

	// Init
	m_ui->layerlist->setEnabled(true);
	updateLockedControls();
}

void LayerList::setLayerEditActions(QAction *addLayer, QAction *addGroup, QAction *duplicate, QAction *merge, QAction *del)
{
	Q_ASSERT(addLayer);
	Q_ASSERT(addGroup);
	Q_ASSERT(duplicate);
	Q_ASSERT(merge);
	Q_ASSERT(del);
	m_addLayerAction = addLayer;
	m_addGroupAction = addGroup;
	m_duplicateLayerAction = duplicate;
	m_mergeLayerAction = merge;
	m_deleteLayerAction = del;

	connect(m_addLayerAction, &QAction::triggered, this, &LayerList::addLayer);
	connect(m_addGroupAction, &QAction::triggered, this, &LayerList::addGroup);
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
	const bool canEdit = m_canvas && m_canvas->aclState()->canUseFeature(canvas::Feature::EditLayers);
	const bool ownLayers = m_canvas && m_canvas->aclState()->canUseFeature(canvas::Feature::OwnLayers);

	// Layer creation actions work as long as we have an editing permission
	const bool canAdd = canEdit | ownLayers;
	const bool hasEditActions = m_addLayerAction != nullptr;
	if(hasEditActions) {
		m_addLayerAction->setEnabled(canAdd);
	}

	// Rest of the controls need a selection to work.
	const bool enabled = m_selectedId && (canEdit || (ownLayers && (m_selectedId>>8) == m_canvas->localUserId()));

	m_ui->lockButton->setEnabled(enabled);
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
		m_opacityUpdateTimer->start(500);
	}
}

static net::Envelope updateLayerAttributesMessage(uint8_t contextId, const canvas::LayerListItem &layer, ChangeFlags<uint8_t> flagChanges, int opacity, int blend)
{
	net::EnvelopeBuilder eb;
	rustpile::write_layerattr(
		eb,
		contextId,
		layer.id,
		0,
		flagChanges.update(layer.attributeFlags()),
		opacity>=0 ? opacity : layer.opacity*255,
		blend >= 0 ? rustpile::Blendmode(blend) : layer.blend
	);
	return eb.toEnvelope();
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
		const auto item = index.data().value<canvas::LayerListItem>();
		int blendmode = m_ui->blendmode->currentData().toInt();

		ChangeFlags<uint8_t> changes;
		changes.set(rustpile::LayerAttributesMessage_FLAGS_ISOLATED, blendmode != -1);
		if(blendmode == -1)
			blendmode = int(item.blend);

		emit layerCommand(updateLayerAttributesMessage(
			m_canvas->localUserId(),
			item,
			changes,
			-1,
			blendmode
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
			ChangeFlags<uint8_t>().set(rustpile::LayerAttributesMessage_FLAGS_CENSOR, censor),
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
			ChangeFlags<uint8_t>().set(rustpile::LayerAttributesMessage_FLAGS_FIXED, fixed),
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
	rustpile::paintengine_set_layer_visibility(
		m_canvas->paintEngine()->engine(),
		layerId,
		visible
	);
}

void LayerList::changeLayerAcl(bool lock, canvas::Tier tier, QVector<uint8_t> exclusive)
{
	const QModelIndex index = currentSelection();
	if(index.isValid()) {
		const int layerId = index.data(canvas::LayerListModel::IdRole).toInt();
		net::EnvelopeBuilder eb;
		rustpile::write_layeracl(
			eb,
			m_canvas->localUserId(),
			layerId,
			(lock ? 0x80 : 0) | uint8_t(tier),
			exclusive.constData(),
			exclusive.length()

		);
		emit layerCommand(eb.toEnvelope());
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
	if(id==0) {
		qWarning("Couldn't find a free ID for a new layer!");
		return;
	}

	const QString name = layers->getAvailableLayerName(tr("Layer"));

	net::EnvelopeBuilder eb;
	rustpile::write_undopoint(eb, m_canvas->localUserId());
	rustpile::write_newlayer(
		eb,
		m_canvas->localUserId(),
		id,
		0, // source
		m_selectedId, // target
		0, // fill
		0, // flags
		reinterpret_cast<const uint16_t*>(name.constData()),
		name.length()
	);
	emit layerCommand(eb.toEnvelope());
}

void LayerList::addGroup()
{
	const canvas::LayerListModel *layers = qobject_cast<canvas::LayerListModel*>(m_ui->layerlist->model());
	Q_ASSERT(layers);

	const int id = layers->getAvailableLayerId();
	if(id==0) {
		qWarning("Couldn't find a free ID for a new group!");
		return;
	}

	const QString name = layers->getAvailableLayerName(tr("Group"));

	net::EnvelopeBuilder eb;
	rustpile::write_undopoint(eb, m_canvas->localUserId());
	rustpile::write_newlayer(
		eb,
		m_canvas->localUserId(),
		id,
		0, // source
		m_selectedId, // target (place above this)
		0, // fill (not used for groups)
		rustpile::LayerCreateMessage_FLAGS_GROUP,
		reinterpret_cast<const uint16_t*>(name.constData()),
		name.length()
	);
	emit layerCommand(eb.toEnvelope());
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

	net::EnvelopeBuilder eb;
	rustpile::write_undopoint(eb, m_canvas->localUserId());
	rustpile::write_newlayer(
		eb,
		m_canvas->localUserId(),
		id,
		layer.id, // source
		layer.id, // target
		0, // fill
		0, // flags
		reinterpret_cast<const uint16_t*>(name.constData()),
		name.length()
	);
	emit layerCommand(eb.toEnvelope());
}

bool LayerList::canMergeCurrent() const
{
	const QModelIndex index = currentSelection();
	const QModelIndex below = index.sibling(index.row()+1, 0);

	return index.isValid() && below.isValid() &&
			!below.data(canvas::LayerListModel::IsGroupRole).toBool() &&
			!m_canvas->aclState()->isLayerLocked(below.data(canvas::LayerListModel::IdRole).toInt())
			;
}

void LayerList::deleteSelected()
{
	QModelIndex index = currentSelection();
	if(!index.isValid())
		return;

	net::EnvelopeBuilder eb;
	rustpile::write_undopoint(eb, m_canvas->localUserId());
	rustpile::write_deletelayer(eb, m_canvas->localUserId(), index.data().value<canvas::LayerListItem>().id, false);
	emit layerCommand(eb.toEnvelope());
}

void LayerList::setSelectedDefault()
{
	QModelIndex index = currentSelection();
	if(!index.isValid())
		return;
	net::EnvelopeBuilder eb;
	rustpile::write_defaultlayer(eb, m_canvas->localUserId(), index.data(canvas::LayerListModel::IdRole).toInt());
	emit layerCommand(eb.toEnvelope());
}

void LayerList::mergeSelected()
{
	QModelIndex index = currentSelection();
	if(!index.isValid())
		return;

	QModelIndex below = index.sibling(index.row()+1, 0);
	if(!below.isValid())
		return;

	net::EnvelopeBuilder eb;
	rustpile::write_undopoint(eb, m_canvas->localUserId());
	rustpile::write_deletelayer(
		eb,
		m_canvas->localUserId(),
		index.data(canvas::LayerListModel::IdRole).value<uint16_t>(),
		below.data(canvas::LayerListModel::IdRole).value<uint16_t>()
	);
	emit layerCommand(eb.toEnvelope());
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

void LayerList::beforeLayerReset()
{
	m_nearestToDeletedId = m_canvas->layerlist()->findNearestLayer(m_selectedId);

	m_expandedGroups.clear();
	for(const auto &item : m_canvas->layerlist()->layerItems()) {
		if(m_ui->layerlist->isExpanded(m_canvas->layerlist()->layerIndex(item.id)))
			m_expandedGroups << item.id;
	}
	m_lastScrollPosition = m_ui->layerlist->verticalScrollBar()->value();
}

void LayerList::afterLayerReset()
{
	const bool wasAnimated = m_ui->layerlist->isAnimated();
	m_ui->layerlist->setAnimated(false);
	if(m_selectedId) {
		const auto selectedIndex = m_canvas->layerlist()->layerIndex(m_selectedId);
		if(selectedIndex.isValid()) {
			selectLayerIndex(selectedIndex);
		} else {
			selectLayer(m_nearestToDeletedId);
		}
	}

	for(const int id : m_expandedGroups)
		m_ui->layerlist->setExpanded(m_canvas->layerlist()->layerIndex(id), true);

	m_ui->layerlist->verticalScrollBar()->setValue(m_lastScrollPosition);
	m_ui->layerlist->setAnimated(wasAnimated);
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
			|| item.group // group layers have no pixel content to edit
			|| m_canvas->aclState()->isLayerLocked(item.id)
			;
			// FIXME: || (m_canvas->layerStack()->isCensored() && item.censored);
	}
	return false;
}

void LayerList::selectionChanged(const QItemSelection &selected)
{
	bool on = selected.count() > 0;

	if(on) {
		updateUiFromSelection();
	} else {
		m_selectedId = 0;
	}

	updateLockedControls();

	emit layerSelected(m_selectedId);
}

void LayerList::updateUiFromSelection()
{
	const canvas::LayerListItem &layer = currentSelection().data().value<canvas::LayerListItem>();
	m_noupdate = true;
	m_selectedId = layer.id;
	m_menuHideAction->setChecked(layer.hidden);
	m_aclmenu->setCensored(layer.censored);
	m_menuDefaultAction->setChecked(currentSelection().data(canvas::LayerListModel::IsDefaultRole).toBool());
	m_menuFixedAction->setChecked(layer.fixed);
	m_ui->opacity->setValue(layer.opacity * 255);

	const int currentBlendmode = m_ui->blendmode->currentData().toInt();

	// "pass-through" is a pseudo-blendmode for layer groups
	const int layerBlendmode = layer.isolated || !layer.group ? int(layer.blend) : -1;

	static_cast<QStandardItemModel*>(m_ui->blendmode->model())->item(m_ui->blendmode->count()-1)->setEnabled(layer.group);

	if(currentBlendmode != layerBlendmode) {
		for(int i=0;i<m_ui->blendmode->count();++i) {
			if(m_ui->blendmode->itemData(i).toInt() == layerBlendmode) {
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

void LayerList::lockStatusChanged(int layerId)
{
	if(m_selectedId == layerId) {
		const auto acl = m_canvas->aclState()->layerAcl(layerId);
		m_ui->lockButton->setChecked(acl.locked || acl.tier != rustpile::Tier::Guest || !acl.exclusive.isEmpty());
		m_aclmenu->setAcl(acl.locked, int(acl.tier), acl.exclusive);
	}
}

void LayerList::emitPropertyChangeCommands(const dialogs::LayerProperties::ChangedLayerData &c)
{
	// Changes that get grouped together into a single layer attribute change message.
	static const unsigned int CHANGE_LAYER_ATTRIBUTES =
			dialogs::LayerProperties::CHANGE_OPACITY |
			dialogs::LayerProperties::CHANGE_BLEND |
			dialogs::LayerProperties::CHANGE_FIXED;

	const uint16_t layerId = c.id;
	const QModelIndex layerIndex = m_canvas->layerlist()->layerIndex(layerId);
	if(!m_canvas || !layerIndex.isValid()) {
		return; // Looks like the layer was deleted from under us.
	}

	const uint8_t contextId = m_canvas->localUserId();
	net::EnvelopeBuilder eb;

	if(c.changes & dialogs::LayerProperties::CHANGE_TITLE) {
		rustpile::write_retitlelayer(eb, contextId, layerId, reinterpret_cast<const uint16_t*>(c.title.constData()), c.title.length());
	}

	if(c.changes & CHANGE_LAYER_ATTRIBUTES) {
		canvas::LayerListItem layer = layerIndex.data().value<canvas::LayerListItem>();

		ChangeFlags<uint8_t> flags;
		if(c.changes & dialogs::LayerProperties::CHANGE_FIXED) {
			flags.set(rustpile::LayerAttributesMessage_FLAGS_FIXED, c.fixed);
		}

		int opacity = -1;
		if(c.changes & dialogs::LayerProperties::CHANGE_OPACITY) {
			opacity = c.opacity * 255;
		}

		int blend = -1;
		if(c.changes & dialogs::LayerProperties::CHANGE_BLEND) {
			blend = int(c.blend);
		}

		emit layerCommand(updateLayerAttributesMessage(
				contextId, layer, flags, opacity, blend));
	}

	if((c.changes & dialogs::LayerProperties::CHANGE_DEFAULT) && c.defaultLayer) {
		rustpile::write_defaultlayer(eb, contextId, layerId);
	}

	emit layerCommand(eb.toEnvelope());
}

}
