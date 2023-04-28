// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "desktop/main.h"
#include "libclient/canvas/blendmodes.h"
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

#include <QDebug>
#include <QComboBox>
#include <QDateTime>
#include <QIcon>
#include <QItemSelection>
#include <QMessageBox>
#include <QPushButton>
#include <QActionGroup>
#include <QTimer>
#include <QStandardItemModel>
#include <QScrollBar>
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
	m_lockButton->setIcon(QIcon::fromTheme("object-locked"));
	m_lockButton->setCheckable(true);
	m_lockButton->setPopupMode(QToolButton::InstantPopup);
	titlebar->addCustomWidget(m_lockButton);
	titlebar->addSpace(4);

	m_blendModeCombo = new QComboBox;
	m_blendModeCombo->setMinimumWidth(24);
	dialogs::LayerProperties::initBlendModeCombo(m_blendModeCombo);
	connect(
		m_blendModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &LayerList::blendModeChanged);
	titlebar->addCustomWidget(m_blendModeCombo, true);
	titlebar->addSpace(4);

	QWidget *root = new QWidget{this};
	QVBoxLayout *rootLayout = new QVBoxLayout;
	rootLayout->setSpacing(0);
	rootLayout->setContentsMargins(0, 0, 0, 0);
	root->setLayout(rootLayout);
	setWidget(root);

	m_opacitySlider = new KisSliderSpinBox{this};
	m_opacitySlider->setRange(0, 100);
	m_opacitySlider->setPrefix(tr("Opacity: "));
	m_opacitySlider->setSuffix(tr("%"));
	m_opacitySlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	m_opacitySlider->setMinimumWidth(24);
	connect(
		m_opacitySlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &LayerList::opacityChanged);
	QHBoxLayout *opacitySliderLayout = new QHBoxLayout;
	opacitySliderLayout->setContentsMargins(
		titleBarWidget()->layout()->contentsMargins());
	opacitySliderLayout->addWidget(m_opacitySlider);
	rootLayout->addLayout(opacitySliderLayout);

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
	connect(canvas->layerlist(), &canvas::LayerListModel::layersVisibleInFrameChanged, this, &LayerList::activeLayerVisibilityChanged);

	connect(canvas->aclState(), &canvas::AclState::featureAccessChanged, this, &LayerList::onFeatureAccessChange);
	connect(canvas->aclState(), &canvas::AclState::layerAclChanged, this, &LayerList::layerLockStatusChanged);
	connect(canvas->aclState(), &canvas::AclState::localLockChanged, this, &LayerList::userLockStatusChanged);
	connect(canvas->aclState(), &canvas::AclState::resetLockChanged, this, &LayerList::setDisabled);
	connect(m_view->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(selectionChanged(QItemSelection)));
	connect(canvas, &canvas::CanvasModel::compatibilityModeChanged, this, &LayerList::updateLockedControls);
	connect(canvas, &canvas::CanvasModel::compatibilityModeChanged, this, &LayerList::updateBlendModes);

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

void LayerList::setLayerEditActions(QAction *addLayer, QAction *addGroup, QAction *duplicate, QAction *merge, QAction *properties, QAction *del, QAction *keyFrameSetLayer)
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
	m_contextMenu->addAction(m_addLayerAction);
	m_contextMenu->addAction(m_addGroupAction);
	m_contextMenu->addAction(m_duplicateLayerAction);
	m_contextMenu->addAction(m_mergeLayerAction);
	m_contextMenu->addAction(m_deleteLayerAction);
	m_contextMenu->addAction(m_propertiesAction);
	m_contextMenu->addSeparator();
	m_contextMenu->addAction(keyFrameSetLayer);

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
			break;
		default: break;
	}
}

void LayerList::updateLockedControls()
{
	// The basic permissions
	canvas::AclState *acls = m_canvas ? m_canvas->aclState() : nullptr;
	const bool locked = acls ? acls->amLocked() : true;
	const bool canEdit = acls && acls->canUseFeature(DP_FEATURE_EDIT_LAYERS);
	const bool ownLayers = acls && acls->canUseFeature(DP_FEATURE_OWN_LAYERS);
	const bool compatibilityMode = m_canvas && m_canvas->isCompatibilityMode();

	// Layer creation actions work as long as we have an editing permission
	const bool canAdd = !locked && (canEdit || ownLayers);
	const bool hasEditActions = m_addLayerAction != nullptr;
	if(hasEditActions) {
		m_addLayerAction->setEnabled(canAdd);
		m_addGroupAction->setEnabled(canAdd && !compatibilityMode);
	}

	// Rest of the controls need a selection to work.
	const bool enabled = !locked && m_selectedId && (canEdit || (ownLayers && (m_selectedId>>8) == m_canvas->localUserId()));

	m_lockButton->setEnabled(enabled);
	m_blendModeCombo->setEnabled(enabled);
	m_opacitySlider->setEnabled(enabled);

	if(hasEditActions) {
		m_duplicateLayerAction->setEnabled(enabled);
		m_deleteLayerAction->setEnabled(enabled);
		m_mergeLayerAction->setEnabled(enabled && canMergeCurrent());
	}
}

void LayerList::updateBlendModes(bool compatibilityMode)
{
	dialogs::LayerProperties::setBlendModeComboCompatibilityMode(
		m_blendModeCombo, compatibilityMode);
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

	drawdance::Message msg;
	if(m_canvas->isCompatibilityMode()) {
		uint16_t targetId = index.isValid() ? m_selectedId : 0;
		uint8_t flags = targetId == 0 ? 0 : DP_MSG_LAYER_CREATE_FLAGS_INSERT;
		msg = drawdance::Message::makeLayerCreate(
			contextId, id, targetId, 0, flags,
			layers->getAvailableLayerName(tr("Layer")));
	} else {
		uint16_t targetId;
		uint8_t flags = group ? DP_MSG_LAYER_TREE_CREATE_FLAGS_GROUP : 0;
		if(index.isValid()) {
			targetId = m_selectedId;
			bool into = index.data(canvas::LayerListModel::IsGroupRole).toBool()
				&& (m_view->isExpanded(index) || index.data(canvas::LayerListModel::IsEmptyRole).toBool());
			if(into) {
				flags |= DP_MSG_LAYER_TREE_CREATE_FLAGS_INTO;
			}
		} else {
			targetId = 0;
		}
		msg = drawdance::Message::makeLayerTreeCreate(
			contextId, id, 0, targetId, 0, flags,
			layers->getAvailableLayerName(group ? tr("Group") : tr("Layer")));
	}

	drawdance::Message messages[] = {
		drawdance::Message::makeUndoPoint(contextId), msg};
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

	drawdance::Message msg;
	if(m_canvas->isCompatibilityMode()) {
		msg = drawdance::Message::makeLayerCreate(
			contextId, id, layer.id, 0,
			DP_MSG_LAYER_CREATE_FLAGS_COPY | DP_MSG_LAYER_CREATE_FLAGS_INSERT,
			layers->getAvailableLayerName(layer.title));
	} else {
		msg = drawdance::Message::makeLayerTreeCreate(
			contextId, id, layer.id, layer.id, 0, 0,
			layers->getAvailableLayerName(layer.title));
	}

	drawdance::Message messages[] = {
		drawdance::Message::makeUndoPoint(contextId), msg};
	emit layerCommands(DP_ARRAY_LENGTH(messages), messages);
}

bool LayerList::canMergeCurrent() const
{
	const QModelIndex index = currentSelection();
	if(!index.isValid()) {
		return false;
	}

	if(index.data(canvas::LayerListModel::IsGroupRole).toBool()) {
		return true;
	} else {
		const QModelIndex below = index.sibling(index.row()+1, 0);
		return below.isValid() &&
			!below.data(canvas::LayerListModel::IsGroupRole).toBool() &&
			!m_canvas->aclState()->isLayerLocked(below.data(canvas::LayerListModel::IdRole).toInt());
	}
}

void LayerList::deleteSelected()
{
	QModelIndex index = currentSelection();
	if(!index.isValid())
		return;

	const canvas::LayerListItem &layer = index.data().value<canvas::LayerListItem>();
	if(dpApp().settings().confirmLayerDelete()) {
		QMessageBox::StandardButton result = QMessageBox::question(
			this, tr("Delete Layer?"),
			tr("Really delete the layer '%1'?").arg(layer.title),
			QMessageBox::Yes | QMessageBox::No,
			QMessageBox::StandardButton::Yes);
		if(result != QMessageBox::StandardButton::Yes) {
			return;
		}
	}

	uint8_t contextId = m_canvas->localUserId();
	drawdance::Message msg;
	if(m_canvas->isCompatibilityMode()) {
		msg = drawdance::Message::makeLayerDelete(
			contextId, index.data().value<canvas::LayerListItem>().id, false);
	} else {
		msg = drawdance::Message::makeLayerTreeDelete(
			contextId, index.data().value<canvas::LayerListItem>().id, 0);
	}

	drawdance::Message messages[] = {
		drawdance::Message::makeUndoPoint(contextId), msg};
	emit layerCommands(DP_ARRAY_LENGTH(messages), messages);
}

void LayerList::mergeSelected()
{
	QModelIndex index = currentSelection();
	if(!index.isValid()) {
		return;
	}

	uint8_t contextId = m_canvas->localUserId();
	int layerId = index.data(canvas::LayerListModel::IdRole).toInt();
	drawdance::Message msg;
	if(m_canvas->isCompatibilityMode()) {
		QModelIndex below = index.sibling(index.row()+1, 0);
		if(!below.isValid()) {
			return;
		}
		msg = drawdance::Message::makeLayerDelete(contextId, layerId, true);
	} else {
		int mergeId;
		if(index.data(canvas::LayerListModel::IsGroupRole).toBool()) {
			mergeId = layerId;
		} else {
			QModelIndex below = index.sibling(index.row()+1, 0);
			if(!below.isValid()) {
				return;
			}
			mergeId = below.data(canvas::LayerListModel::IdRole).toInt();
		}
		msg = drawdance::Message::makeLayerTreeDelete(contextId, layerId, mergeId);
	}

	drawdance::Message messages[] = {
		drawdance::Message::makeUndoPoint(contextId), msg};
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

		connect(m_canvas, &canvas::CanvasModel::compatibilityModeChanged, dlg,
			&dialogs::LayerProperties::setCompatibilityMode);
		connect(dlg, &dialogs::LayerProperties::layerCommands, this, &LayerList::layerCommands);
		connect(dlg, &dialogs::LayerProperties::visibilityChanged, this, &LayerList::setLayerVisibility);
		connect(m_canvas->layerlist(), &canvas::LayerListModel::modelReset, dlg, [this, dlg]() {
			const auto newIndex = m_canvas->layerlist()->layerIndex(dlg->layerId());
			if(newIndex.isValid()) {
				dlg->setLayerItem(
					newIndex.data().value<canvas::LayerListItem>(),
					layerCreatorName(dlg->layerId()),
					newIndex.data(canvas::LayerListModel::IsDefaultRole).toBool()
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
		dlg->setCompatibilityMode(m_canvas->isCompatibilityMode());

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
	m_lastScrollPosition = m_view->verticalScrollBar()->value();
	// We want to retain the expanded state of layer groups across resets, but
	// that's pretty tricky, since we might get hit with intermediate states
	// where the groups don't exist at all. We also don't want to expand groups
	// that just happen to have the same ids as one that used to be expanded
	// ages ago. So we use some heuristics here. First, we clear out all groups
	// that are collapsed, since we might have some stale entries from earlier.
	for(int layerId : m_expandedGroups.keys()) {
		QModelIndex index = m_canvas->layerlist()->layerIndex(layerId);
		if(index.isValid() && !m_view->isExpanded(index)) {
			m_expandedGroups.remove(layerId);
		}
	}
	// Now we collect all expanded groups and remember the time at which we saw
	// them as well as their current title. When deciding whether to expand a
	// group, we use those to guess if this is still the same group or not.
	qint64 now = QDateTime::currentMSecsSinceEpoch();
	for(const canvas::LayerListItem &item : m_canvas->layerlist()->layerItems()) {
		if(m_view->isExpanded(m_canvas->layerlist()->layerIndex(item.id))) {
			m_expandedGroups.insert(item.id, {item.title, now});
		}
	}
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

	// Heuristic for recognizing if the group we're dealing with is still the
	// same. We'll say that after 1 minute, it's probably a different group.
	// This should also cover cases where the group is deleted and then that
	// deletion is undone or something. This might incorrectly expand a group
	// that had a default "Group 1" name or something that was deleted and then
	// recreated shortly after, but that's rare and non-disruptive, so whatever.
	constexpr qint64 MAX_TIME = 60000;
	qint64 now = QDateTime::currentMSecsSinceEpoch();
	for(int layerId : m_expandedGroups.keys()) {
		QModelIndex index = m_canvas->layerlist()->layerIndex(layerId);
		if(index.isValid()) {
			QPair<QString, qint64> pair = m_expandedGroups.value(layerId);
			bool sameTitle = index.data(canvas::LayerListModel::TitleRole).toString() == pair.first;
			bool inTime = now - pair.second <= MAX_TIME;
			if(sameTitle && inTime) {
				m_view->setExpanded(index, true);
			}
		}
	}

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
			|| idx.data(canvas::LayerListModel::IsLockedRole).toBool()
			|| (item.censored && m_canvas->paintEngine()->isCensored());
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

	layerLockStatusChanged(layer.id);
	updateLockedControls();

	// TODO use change flags to detect if this really changed
	emit activeLayerVisibilityChanged();
	m_noupdate = false;
}

void LayerList::layerLockStatusChanged(int layerId)
{
	if(m_selectedId == layerId) {
		const auto acl = m_canvas->aclState()->layerAcl(layerId);
		m_lockButton->setChecked(acl.locked || acl.tier != DP_ACCESS_TIER_GUEST || !acl.exclusive.isEmpty());
		m_aclmenu->setAcl(acl.locked, int(acl.tier), acl.exclusive);

		emit activeLayerVisibilityChanged();
	}
}

void LayerList::userLockStatusChanged(bool)
{
	updateLockedControls();
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
