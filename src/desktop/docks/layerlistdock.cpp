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

#include "desktop/widgets/groupedtoolbutton.h"
#include "libclient/canvas/layerlist.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/userlist.h"
#include "libclient/canvas/paintengine.h"
#include "desktop/docks/layerlistdock.h"
#include "desktop/docks/layerlistdelegate.h"
#include "desktop/docks/layeraclmenu.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/dialogs/layerproperties.h"
#include "libclient/utils/changeflags.h"
#include "libclient/utils/icon.h"
#include "libclient/net/envelopebuilder.h"

#include "rustpile/rustpile.h"

#include <QDebug>
#include <QItemSelection>
#include <QMessageBox>
#include <QPushButton>
#include <QActionGroup>
#include <QTimer>
#include <QSettings>
#include <QStandardItemModel>
#include <QScrollBar>
#include <QTreeView>

namespace docks {

LayerList::LayerList(QWidget *parent)
	: QDockWidget(tr("Layers"), parent),
	  m_canvas(nullptr), m_selectedId(0), m_nearestToDeletedId(0),
	  m_noupdate(false),
	  m_addLayerAction(nullptr), m_duplicateLayerAction(nullptr),
	  m_mergeLayerAction(nullptr), m_deleteLayerAction(nullptr)
{
	auto *titlebar = new TitleWidget(this);
	setTitleBarWidget(titlebar);

	m_lockButton = new widgets::GroupedToolButton(widgets::GroupedToolButton::NotGrouped, titlebar);
	m_lockButton->setIcon(icon::fromTheme("object-locked"));
	m_lockButton->setCheckable(true);
	m_lockButton->setPopupMode(QToolButton::InstantPopup);
	titlebar->addCustomWidget(m_lockButton);
	titlebar->addStretch();

	m_view = new QTreeView;
	m_view->setHeaderHidden(true);
	setWidget(m_view);

	m_view->setDragEnabled(true);
	m_view->viewport()->setAcceptDrops(true);
	m_view->setEnabled(false);
	m_view->setSelectionMode(QAbstractItemView::SingleSelection);
	m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_view->setContextMenuPolicy(Qt::CustomContextMenu);

	m_contextMenu = new QMenu(this);
	connect(m_view, &QTreeView::customContextMenuRequested, this, &LayerList::showContextMenu);

	// Layer ACL menu
	m_aclmenu = new LayerAclMenu(this);
	m_lockButton->setMenu(m_aclmenu);

	connect(m_aclmenu, &LayerAclMenu::layerAclChange, this, &LayerList::changeLayerAcl);
	connect(m_aclmenu, &LayerAclMenu::layerCensoredChange, this, &LayerList::censorSelected);

	selectionChanged(QItemSelection());

	// Custom layer list item delegate
	LayerListDelegate *del = new LayerListDelegate(this);
	connect(del, &LayerListDelegate::toggleVisibility, this, &LayerList::setLayerVisibility);
	connect(del, &LayerListDelegate::editProperties, this, &LayerList::showPropertiesOfIndex);
	m_view->setItemDelegate(del);
}


void LayerList::setCanvas(canvas::CanvasModel *canvas)
{
	m_canvas = canvas;
	m_view->setModel(canvas->layerlist());

	m_aclmenu->setUserList(canvas->userlist()->onlineUsers());

	connect(canvas->layerlist(), &canvas::LayerListModel::modelAboutToBeReset, this, &LayerList::beforeLayerReset);
	connect(canvas->layerlist(), &canvas::LayerListModel::modelReset, this, &LayerList::afterLayerReset);

	connect(canvas->aclState(), &canvas::AclState::featureAccessChanged, this, &LayerList::onFeatureAccessChange);
	connect(canvas->aclState(), &canvas::AclState::layerAclChanged, this, &LayerList::lockStatusChanged);
	connect(m_view->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(selectionChanged(QItemSelection)));

	// Init
	m_view->setEnabled(true);
	updateLockedControls();
}

void LayerList::setLayerEditActions(QAction *addLayer, QAction *addGroup, QAction *duplicate, QAction *merge, QAction *properties, QAction *del)
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
	m_propertiesAction = properties;
	m_deleteLayerAction = del;

	// Add the actions to the header bar
	TitleWidget *titlebar = qobject_cast<TitleWidget*>(titleBarWidget());
	Q_ASSERT(titlebar);

	auto *addLayerButton = new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft, titlebar);
	addLayerButton->setDefaultAction(addLayer);
	titlebar->addCustomWidget(addLayerButton);

	auto *addGroupButton = new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter, titlebar);
	addGroupButton->setDefaultAction(addGroup);
	titlebar->addCustomWidget(addGroupButton);

	auto *dupplicateLayerButton = new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter, titlebar);
	dupplicateLayerButton->setDefaultAction(m_duplicateLayerAction);
	titlebar->addCustomWidget(dupplicateLayerButton);

	auto *mergeLayerButton = new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter, titlebar);
	mergeLayerButton->setDefaultAction(m_mergeLayerAction);
	titlebar->addCustomWidget(mergeLayerButton);

	auto *propertiesButton = new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter, titlebar);
	propertiesButton->setDefaultAction(m_propertiesAction);
	titlebar->addCustomWidget(propertiesButton);

	auto *deleteLayerButton = new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight, titlebar);
	deleteLayerButton->setDefaultAction(m_deleteLayerAction);
	titlebar->addCustomWidget(deleteLayerButton);

	titlebar->addStretch();

	// Add the actions to the context menu
	m_contextMenu->addAction(m_propertiesAction);
	m_contextMenu->addSeparator();
	m_contextMenu->addAction(m_addLayerAction);
	m_contextMenu->addAction(m_addGroupAction);
	m_contextMenu->addAction(m_duplicateLayerAction);
	m_contextMenu->addAction(m_mergeLayerAction);
	m_contextMenu->addAction(m_deleteLayerAction);

	// Action functionality
	connect(m_addLayerAction, &QAction::triggered, this, &LayerList::addLayer);
	connect(m_addGroupAction, &QAction::triggered, this, &LayerList::addGroup);
	connect(m_duplicateLayerAction, &QAction::triggered, this, &LayerList::duplicateLayer);
	connect(m_mergeLayerAction, &QAction::triggered, this, &LayerList::mergeSelected);
	connect(m_propertiesAction, &QAction::triggered, this, &LayerList::showPropertiesOfSelected);
	connect(m_deleteLayerAction, &QAction::triggered, this, &LayerList::deleteSelected);

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

	m_lockButton->setEnabled(enabled);

	if(hasEditActions) {
		m_duplicateLayerAction->setEnabled(enabled);
		m_propertiesAction->setEnabled(enabled);
		m_deleteLayerAction->setEnabled(enabled);
		m_mergeLayerAction->setEnabled(enabled && canMergeCurrent());
	}
}

void LayerList::selectLayer(int id)
{
	selectLayerIndex(m_canvas->layerlist()->layerIndex(id), true);
}

void LayerList::selectLayerIndex(QModelIndex index, bool scrollTo)
{
	if(index.isValid()) {
		m_view->selectionModel()->select(
			index, QItemSelectionModel::SelectCurrent|QItemSelectionModel::Clear);
		if(scrollTo) {
			m_view->setExpanded(index, true);
			m_view->scrollTo(index);
		}
	}
}

QString LayerList::layerCreatorName(uint16_t layerId) const
{
	return m_canvas->userlist()->getUsername((layerId >> 8) & 0xff);
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

/**
 * @brief Layer add button pressed
 */
void LayerList::addLayer()
{
	const canvas::LayerListModel *layers = m_canvas->layerlist();
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
	const canvas::LayerListModel *layers = m_canvas->layerlist();
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

	const canvas::LayerListModel *layers = m_canvas->layerlist();
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
		auto *dlg = new dialogs::LayerProperties(m_canvas->localUserId(), this);
		dlg->setAttribute(Qt::WA_DeleteOnClose);
		dlg->setModal(false);

		connect(dlg, &dialogs::LayerProperties::layerCommand, this, &LayerList::layerCommand);
		connect(dlg, &dialogs::LayerProperties::visibilityChanged, this, &LayerList::setLayerVisibility);
		connect(m_canvas->layerlist(), &canvas::LayerListModel::modelReset, dlg, [this, dlg]() {
			const auto index = m_canvas->layerlist()->layerIndex(dlg->layerId());
			if(index.isValid()) {
				dlg->setLayerItem(
					index.data().value<canvas::LayerListItem>(),
					layerCreatorName(dlg->layerId()),
					index.data(canvas::LayerListModel::IsDefaultRole).toBool()
				);
			} else {
				dlg->deleteLater();
			}
		});

		const int layerId = index.data(canvas::LayerListModel::IdRole).toInt();
		dlg->setLayerItem(
			index.data().value<canvas::LayerListItem>(),
			layerCreatorName(layerId),
			index.data(canvas::LayerListModel::IsDefaultRole).toBool()
		);

		const bool canEditAll = m_canvas->aclState()->canUseFeature(canvas::Feature::EditLayers);
		const bool canEdit = canEditAll ||
			(
				m_canvas->aclState()->canUseFeature(canvas::Feature::OwnLayers) &&
				(layerId & 0xff00) >> 8 == m_canvas->localUserId()
			);
		dlg->setControlsEnabled(canEdit);
		dlg->setOpControlsEnabled(canEditAll);

		dlg->show();
	}
}

void LayerList::showContextMenu(const QPoint &pos)
{
	QModelIndex index = m_view->indexAt(pos);
	if(index.isValid()) {
		m_contextMenu->popup(m_view->mapToGlobal(pos));
	}
}

void LayerList::beforeLayerReset()
{
	m_nearestToDeletedId = m_canvas->layerlist()->findNearestLayer(m_selectedId);

	m_expandedGroups.clear();
	for(const auto &item : m_canvas->layerlist()->layerItems()) {
		if(m_view->isExpanded(m_canvas->layerlist()->layerIndex(item.id)))
			m_expandedGroups << item.id;
	}
	m_lastScrollPosition = m_view->verticalScrollBar()->value();
}

void LayerList::afterLayerReset()
{
	const bool wasAnimated = m_view->isAnimated();
	m_view->setAnimated(false);
	if(m_selectedId) {
		const auto selectedIndex = m_canvas->layerlist()->layerIndex(m_selectedId);
		if(selectedIndex.isValid()) {
			selectLayerIndex(selectedIndex);
		} else {
			selectLayer(m_nearestToDeletedId);
		}
	}

	for(const int id : qAsConst(m_expandedGroups))
		m_view->setExpanded(m_canvas->layerlist()->layerIndex(id), true);

	m_view->verticalScrollBar()->setValue(m_lastScrollPosition);
	m_view->setAnimated(wasAnimated);
}

QModelIndex LayerList::currentSelection() const
{
	QModelIndexList sel = m_view->selectionModel()->selectedIndexes();
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
			|| (item.censored && m_canvas->paintEngine()->isCensored())
			;
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

	m_aclmenu->setCensored(layer.censored);

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
		m_lockButton->setChecked(acl.locked || acl.tier != rustpile::Tier::Guest || !acl.exclusive.isEmpty());
		m_aclmenu->setAcl(acl.locked, int(acl.tier), acl.exclusive);

		emit activeLayerVisibilityChanged();
	}
}

}
