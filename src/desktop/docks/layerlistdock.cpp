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

#include "widgets/groupedtoolbutton.h"
#include "canvas/blendmodes.h"
#include "canvas/layerlist.h"
#include "canvas/canvasmodel.h"
#include "canvas/userlist.h"
#include "canvas/paintengine.h"
#include "docks/layerlistdock.h"
#include "docks/layerlistdelegate.h"
#include "docks/layeraclmenu.h"
#include "docks/titlewidget.h"
#include "dialogs/layerproperties.h"
#include "utils/changeflags.h"
#include "utils/icon.h"

#include <QDebug>
#include <QComboBox>
#include <QItemSelection>
#include <QMessageBox>
#include <QPushButton>
#include <QActionGroup>
#include <QTimer>
#include <QSettings>
#include <QStandardItemModel>
#include <QScrollBar>
#include <QSpinBox>
#include <QTreeView>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace docks {

LayerList::LayerList(QWidget *parent)
	: QDockWidget(tr("Layers"), parent),
	  m_canvas(nullptr), m_selectedId(0), m_nearestToDeletedId(0),
	  m_noupdate(false), m_updateBlendModeIndex(-1), m_updateOpacity(-1),
	  m_addLayerAction(nullptr), m_duplicateLayerAction(nullptr),
	  m_mergeLayerAction(nullptr), m_deleteLayerAction(nullptr)
{
	m_debounceTimer = new QTimer{this};
	m_debounceTimer->setSingleShot(true);
	m_debounceTimer->setInterval(500);
	connect(m_debounceTimer, &QTimer::timeout, this, &LayerList::triggerUpdate);

	auto *titlebar = new TitleWidget(this);
	setTitleBarWidget(titlebar);

	m_lockButton = new widgets::GroupedToolButton{widgets::GroupedToolButton::NotGrouped};
	m_lockButton->setIcon(icon::fromTheme("object-locked"));
	m_lockButton->setCheckable(true);
	m_lockButton->setPopupMode(QToolButton::InstantPopup);
	titlebar->addCustomWidget(m_lockButton);
	titlebar->addSpace(4);

	m_blendModeCombo = new QComboBox;
	for(const QPair<DP_BlendMode, QString> &bm : canvas::blendmode::layerModeNames()) {
		m_blendModeCombo->addItem(bm.second, int(bm.first));
	}
	connect(
		m_blendModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &LayerList::blendModeChanged);
	titlebar->addCustomWidget(m_blendModeCombo);
	titlebar->addSpace(4);

	m_opacitySlider = new QSlider{Qt::Horizontal};
	m_opacitySlider->setRange(0, 100);
	connect(m_opacitySlider, &QSlider::valueChanged, this, &LayerList::opacityChanged);
	titlebar->addCustomWidget(m_opacitySlider);
	titlebar->addSpace(4);

	QWidget *root = new QWidget{this};
	QVBoxLayout *rootLayout = new QVBoxLayout;
	rootLayout->setSpacing(0);
	rootLayout->setContentsMargins(0, 0, 0, 0);
	root->setLayout(rootLayout);
	setWidget(root);

	m_view = new QTreeView;
	m_view->setHeaderHidden(true);
	m_view->setDragEnabled(true);
	m_view->viewport()->setAcceptDrops(true);
	m_view->setEnabled(false);
	m_view->setSelectionMode(QAbstractItemView::SingleSelection);
	m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_view->setContextMenuPolicy(Qt::CustomContextMenu);
	rootLayout->addWidget(m_view);

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

static void addLayerButton(
	QWidget *root, QHBoxLayout *layout, QAction *action,
	widgets::GroupedToolButton::GroupPosition position)
{
	widgets::GroupedToolButton *button = new widgets::GroupedToolButton{position, root};
	button->setDefaultAction(action);
	layout->addWidget(button);
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

	// Add the actions below the layer list
	QWidget *root = widget();
	Q_ASSERT(root);
	Q_ASSERT(titleBarWidget());

	QHBoxLayout *layout = new QHBoxLayout;
	layout->setSpacing(0);
	layout->setContentsMargins(titleBarWidget()->layout()->contentsMargins());
	addLayerButton(root, layout, addLayer, widgets::GroupedToolButton::GroupLeft);
	addLayerButton(root, layout, addGroup, widgets::GroupedToolButton::GroupCenter);
	addLayerButton(root, layout, duplicate, widgets::GroupedToolButton::GroupCenter);
	addLayerButton(root, layout, merge, widgets::GroupedToolButton::GroupCenter);
	addLayerButton(root, layout, properties, widgets::GroupedToolButton::GroupRight);
	layout->addStretch();
	addLayerButton(root, layout, del, widgets::GroupedToolButton::NotGrouped);
	root->layout()->addItem(layout);

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

void LayerList::onFeatureAccessChange(DP_Feature feature, bool canUse)
{
	Q_UNUSED(canUse);
	switch(feature) {
		case DP_FEATURE_EDIT_LAYERS:
		case DP_FEATURE_OWN_LAYERS:
			updateLockedControls();
		default: break;
	}
}

void LayerList::updateLockedControls()
{
	// The basic permissions
	const bool canEdit = m_canvas && m_canvas->aclState()->canUseFeature(DP_FEATURE_EDIT_LAYERS);
	const bool ownLayers = m_canvas && m_canvas->aclState()->canUseFeature(DP_FEATURE_OWN_LAYERS);

	// Layer creation actions work as long as we have an editing permission
	const bool canAdd = canEdit | ownLayers;
	const bool hasEditActions = m_addLayerAction != nullptr;
	if(hasEditActions) {
		m_addLayerAction->setEnabled(canAdd);
		m_addGroupAction->setEnabled(canAdd);
	}

	// Rest of the controls need a selection to work.
	const bool enabled = m_selectedId && (canEdit || (ownLayers && (m_selectedId>>8) == m_canvas->localUserId()));

	m_lockButton->setEnabled(enabled);
	m_blendModeCombo->setEnabled(enabled);
	m_opacitySlider->setEnabled(enabled);

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

void LayerList::selectAbove()
{
	selectLayerIndex(m_view->indexAbove(currentSelection()), true);
}

void LayerList::selectBelow()
{
	selectLayerIndex(m_view->indexBelow(currentSelection()), true);
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

void LayerList::censorSelected(bool censor)
{
	QModelIndex index = currentSelection();
	if(index.isValid()) {
		canvas::LayerListItem layer = index.data().value<canvas::LayerListItem>();
		uint8_t flags = ChangeFlags<uint8_t>()
			.set(DP_MSG_LAYER_ATTRIBUTES_FLAGS_CENSOR, censor)
			.update(layer.attributeFlags());
		drawdance::Message msg = drawdance::Message::makeLayerAttributes(
			m_canvas->localUserId(), layer.id, 0, flags, layer.opacity * 255, layer.blend);
		emit layerCommands(1, &msg);
	}
}

void LayerList::setLayerVisibility(int layerId, bool visible)
{
	m_canvas->paintEngine()->setLayerVisibility(layerId, !visible);
}

void LayerList::changeLayerAcl(bool lock, DP_AccessTier tier, QVector<uint8_t> exclusive)
{
	const QModelIndex index = currentSelection();
	if(index.isValid()) {
		uint16_t layerId = index.data(canvas::LayerListModel::IdRole).toInt();
		uint8_t flags = (lock ? DP_ACL_ALL_LOCKED_BIT : 0) | uint8_t(tier);
		drawdance::Message msg = drawdance::Message::makeLayerAcl(
			m_canvas->localUserId(), layerId, flags, exclusive);
		emit layerCommands(1, &msg);
	}
}

/**
 * @brief Layer add button pressed
 */
void LayerList::addLayer()
{
	addLayerOrGroup(false);
}

void LayerList::addGroup()
{
	addLayerOrGroup(true);
}

void LayerList::addLayerOrGroup(bool group)
{
	const canvas::LayerListModel *layers = m_canvas->layerlist();
	Q_ASSERT(layers);

	const int id = layers->getAvailableLayerId();
	if(id==0) {
		qWarning("Couldn't find a free ID for a new %s!", group ? "group" : "layer");
		return;
	}

	uint8_t contextId = m_canvas->localUserId();
	QModelIndex index = layers->layerIndex(m_selectedId);
	uint16_t targetId;
	uint8_t flags = group ? DP_MSG_LAYER_CREATE_FLAGS_GROUP : 0;
	if(index.isValid()) {
		targetId = m_selectedId;
		if(index.data(canvas::LayerListModel::IsGroupRole).toBool() && m_view->isExpanded(index)) {
			flags |= DP_MSG_LAYER_CREATE_FLAGS_INTO;
		}
	} else {
		targetId = 0;
	}
	drawdance::Message messages[] = {
		drawdance::Message::makeUndoPoint(contextId),
		drawdance::Message::makeLayerCreate(
			contextId, id, 0, targetId, 0, flags,
			layers->getAvailableLayerName(group ? tr("Group") : tr("Layer"))),
	};
	emit layerCommands(DP_ARRAY_LENGTH(messages), messages);
}

void LayerList::duplicateLayer()
{
	const QModelIndex index = currentSelection();
	const canvas::LayerListItem layer = index.data().value<canvas::LayerListItem>();

	const canvas::LayerListModel *layers = m_canvas->layerlist();
	Q_ASSERT(layers);

	const int id = layers->getAvailableLayerId();
	if(id==0) {
		qWarning("Couldn't find a free ID for duplicating layer!");
		return;
	}

	uint8_t contextId = m_canvas->localUserId();
	drawdance::Message messages[] = {
		drawdance::Message::makeUndoPoint(contextId),
		drawdance::Message::makeLayerCreate(
			contextId, id, layer.id, layer.id, 0, 0,
			layers->getAvailableLayerName(layer.title)),
	};
	emit layerCommands(DP_ARRAY_LENGTH(messages), messages);
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

	uint8_t contextId = m_canvas->localUserId();
	drawdance::Message messages[] = {
		drawdance::Message::makeUndoPoint(contextId),
		drawdance::Message::makeLayerDelete(
			contextId, index.data().value<canvas::LayerListItem>().id, 0),
	};
	emit layerCommands(DP_ARRAY_LENGTH(messages), messages);
}

void LayerList::mergeSelected()
{
	QModelIndex index = currentSelection();
	if(!index.isValid())
		return;

	QModelIndex below = index.sibling(index.row()+1, 0);
	if(!below.isValid())
		return;

	uint8_t contextId = m_canvas->localUserId();
	drawdance::Message messages[] = {
		drawdance::Message::makeUndoPoint(contextId),
		drawdance::Message::makeLayerDelete(contextId,
			index.data(canvas::LayerListModel::IdRole).value<uint16_t>(),
			below.data(canvas::LayerListModel::IdRole).value<uint16_t>()),
	};
	emit layerCommands(DP_ARRAY_LENGTH(messages), messages);
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

		connect(dlg, &dialogs::LayerProperties::layerCommands, this, &LayerList::layerCommands);
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

		const bool canEditAll = m_canvas->aclState()->canUseFeature(DP_FEATURE_EDIT_LAYERS);
		const bool canEdit = canEditAll ||
			(
				m_canvas->aclState()->canUseFeature(DP_FEATURE_OWN_LAYERS) &&
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

	if(m_canvas->layerlist()->rowCount() == 0) {
		// If there's nothing to select, the selection model doesn't emit
		// a selection change signal, so we have to call this manually.
		selectionChanged(QItemSelection{});
	} else if(m_selectedId) {
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
	if(m_debounceTimer->isActive()) {
		m_debounceTimer->stop();
		triggerUpdate();
	}

	bool on = selected.count() > 0;

	if(on) {
		updateUiFromSelection();
	} else {
		m_selectedId = 0;
	}

	updateLockedControls();

	emit layerSelected(m_selectedId);
}

static int searchBlendModeIndex(QComboBox *blendModeCombo, DP_BlendMode mode)
{
	const int blendModeCount = blendModeCombo->count();
    for(int i = 0; i < blendModeCount; ++i) {
		if(blendModeCombo->itemData(i).toInt() == int(mode)) {
            return i;
        }
    }
    return 0; // Don't know that blend mode, punt to Normal.
}

void LayerList::updateUiFromSelection()
{
	const canvas::LayerListItem &layer = currentSelection().data().value<canvas::LayerListItem>();
	m_noupdate = true;
	m_selectedId = layer.id;

	m_aclmenu->setCensored(layer.censored);
	m_blendModeCombo->setCurrentIndex(searchBlendModeIndex(m_blendModeCombo, layer.blend));
	m_opacitySlider->setValue(layer.opacity * 100.0 + 0.5);

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
		m_lockButton->setChecked(acl.locked || acl.tier != DP_ACCESS_TIER_GUEST || !acl.exclusive.isEmpty());
		m_aclmenu->setAcl(acl.locked, int(acl.tier), acl.exclusive);

		emit activeLayerVisibilityChanged();
	}
}

void LayerList::blendModeChanged(int index)
{
	if(m_noupdate) {
		return;
	}
	m_updateBlendModeIndex = index;
	m_debounceTimer->start();
}

void LayerList::opacityChanged(int value)
{
	if(m_noupdate) {
		return;
	}
	m_updateOpacity = value;
	m_debounceTimer->start();
}

void LayerList::triggerUpdate()
{
	QModelIndex index = m_canvas->layerlist()->layerIndex(m_selectedId);
	if(!index.isValid()) {
		return;
	}
	const canvas::LayerListItem &layer = index.data().value<canvas::LayerListItem>();

	DP_BlendMode mode;
	if(m_updateBlendModeIndex == -1) {
		mode = layer.blend;
	} else {
		mode = DP_BlendMode(m_blendModeCombo->itemData(m_updateBlendModeIndex).toInt());
		m_updateBlendModeIndex = -1;
	}

	float opacity;
	if(m_updateOpacity == -1) {
		opacity = layer.opacity;
	} else {
		opacity = m_updateOpacity / 100.0f;
		m_updateOpacity = -1;
	}

	uint8_t flags =
		(layer.censored ? DP_MSG_LAYER_ATTRIBUTES_FLAGS_CENSOR : 0) |
		(layer.isolated ? DP_MSG_LAYER_ATTRIBUTES_FLAGS_ISOLATED : 0);

	drawdance::Message msg = drawdance::Message::makeLayerAttributes(
		m_canvas->localUserId(), layer.id, 0, flags, opacity * 255.0f + 0.5f, mode);

	emit layerCommands(1, &msg);
}

}
