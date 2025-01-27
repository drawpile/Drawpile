// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/docks/layerlistdock.h"
#include "desktop/dialogs/colordialog.h"
#include "desktop/dialogs/layerproperties.h"
#include "desktop/docks/layeraclmenu.h"
#include "desktop/docks/layerlistdelegate.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/layerlist.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/canvas/timelinemodel.h"
#include "libclient/canvas/transformmodel.h"
#include "libclient/canvas/userlist.h"
#include "libclient/utils/changeflags.h"
#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QDateTime>
#include <QDebug>
#include <QHBoxLayout>
#include <QIcon>
#include <QItemSelection>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>
#include <functional>

namespace docks {

LayerList::LayerList(QWidget *parent)
	: DockBase(
		  tr("Layers"), QString(), QIcon::fromTheme("layer-visible-on"), parent)
	, m_canvas(nullptr)
	, m_selectedId(0)
	, m_nearestToDeletedId(0)
	, m_trackId(0)
	, m_frame(-1)
	, m_noupdate(false)
	, m_updateBlendModeIndex(-1)
	, m_updateOpacity(-1)
{
	m_debounceTimer = new QTimer{this};
	m_debounceTimer->setSingleShot(true);
	m_debounceTimer->setInterval(dpApp().settings().debounceDelayMs());
	connect(m_debounceTimer, &QTimer::timeout, this, &LayerList::triggerUpdate);

	auto *titlebar = new TitleWidget(this);
	setTitleBarWidget(titlebar);

	m_lockButton =
		new widgets::GroupedToolButton{widgets::GroupedToolButton::NotGrouped};
	m_lockButton->setIcon(QIcon::fromTheme("object-locked"));
	m_lockButton->setCheckable(true);
	m_lockButton->setPopupMode(QToolButton::InstantPopup);
	titlebar->addCustomWidget(m_lockButton);
	titlebar->addSpace(4);

	m_blendModeCombo = new QComboBox;
	m_blendModeCombo->setMinimumWidth(24);
	utils::setWidgetRetainSizeWhenHidden(m_blendModeCombo, true);
	connect(
		m_blendModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &LayerList::blendModeChanged);
	titlebar->addCustomWidget(m_blendModeCombo, 1);
	titlebar->addSpace(4);

	QWidget *root = new QWidget{this};
	QVBoxLayout *rootLayout = new QVBoxLayout;
	rootLayout->setSpacing(0);
	rootLayout->setContentsMargins(0, 0, 0, 0);
	root->setLayout(rootLayout);
	setWidget(root);

	m_sketchButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::NotGrouped);
	m_sketchButton->setIcon(QIcon::fromTheme("draw-freehand"));
	m_sketchButton->setToolTip(tr("Toggle sketch mode (only visible to you)"));
	m_sketchButton->setStatusTip(m_sketchButton->toolTip());
	m_sketchButton->setCheckable(true);
	connect(
		m_sketchButton, &widgets::GroupedToolButton::clicked, this,
		&LayerList::toggleLayerSketch);

	m_opacitySlider = new KisSliderSpinBox{this};
	m_opacitySlider->setRange(0, 100);
	m_opacitySlider->setPrefix(tr("Opacity: "));
	m_opacitySlider->setSuffix(tr("%"));
	m_opacitySlider->setSizePolicy(
		QSizePolicy::Expanding, QSizePolicy::Preferred);
	m_opacitySlider->setMinimumWidth(24);
	connect(
		m_opacitySlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &LayerList::opacityChanged);

	m_sketchTintButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	m_sketchTintButton->setToolTip(tr("Change sketch tint"));
	m_sketchTintButton->setStatusTip(m_sketchButton->toolTip());
	m_sketchTintButton->setEnabled(false);
	m_sketchTintButton->setVisible(false);
	connect(
		m_sketchTintButton, &widgets::GroupedToolButton::clicked, this,
		&LayerList::showSketchTintColorPicker);

	QHBoxLayout *opacitySliderLayout = new QHBoxLayout;
	opacitySliderLayout->setContentsMargins(
		titlebar->layout()->contentsMargins());
	opacitySliderLayout->addWidget(m_sketchButton);
	opacitySliderLayout->addWidget(m_sketchTintButton);
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
	utils::bindKineticScrolling(m_view);
	rootLayout->addWidget(m_view);

	m_contextMenu = new QMenu(this);
	connect(
		m_view, &QTreeView::customContextMenuRequested, this,
		&LayerList::showContextMenu);

	// Layer ACL menu
	m_aclmenu = new LayerAclMenu(this);
	m_lockButton->setMenu(m_aclmenu);

	connect(
		m_aclmenu, &LayerAclMenu::layerAclChange, this,
		&LayerList::changeLayerAcl);
	connect(
		m_aclmenu, &LayerAclMenu::layerCensoredChange, this,
		&LayerList::censorSelected);

	selectionChanged(QItemSelection());

	// Custom layer list item delegate
	LayerListDelegate *del = new LayerListDelegate(this);
	connect(
		del, &LayerListDelegate::interacted, this,
		&LayerList::disableAutoselectAny);
	connect(
		del, &LayerListDelegate::toggleVisibility, this,
		&LayerList::setLayerVisibility);
	connect(
		del, &LayerListDelegate::toggleChecked, this, &LayerList::layerChecked);
	connect(
		del, &LayerListDelegate::editProperties, this,
		&LayerList::showPropertiesOfIndex);
	m_view->setItemDelegate(del);
}


void LayerList::setCanvas(canvas::CanvasModel *canvas)
{
	m_canvas = canvas;
	m_view->setModel(canvas->layerlist());

	m_aclmenu->setUserList(canvas->userlist()->onlineUsers());

	connect(
		canvas->layerlist(), &canvas::LayerListModel::modelAboutToBeReset, this,
		&LayerList::beforeLayerReset);
	connect(
		canvas->layerlist(), &canvas::LayerListModel::modelReset, this,
		&LayerList::afterLayerReset);
	connect(
		canvas->layerlist(), &canvas::LayerListModel::layerCheckStateToggled,
		this, &LayerList::updateActionLabels);
	connect(
		canvas->layerlist(), &canvas::LayerListModel::layerCheckStateToggled,
		this, &LayerList::updateCheckActions);
	connect(
		canvas->layerlist(),
		&canvas::LayerListModel::layersVisibleInFrameChanged, this,
		&LayerList::activeLayerVisibilityChanged);
	connect(
		this, &LayerList::layerChecked, canvas->layerlist(),
		&canvas::LayerListModel::setLayerChecked);

	connect(
		canvas->aclState(), &canvas::AclState::featureAccessChanged, this,
		&LayerList::onFeatureAccessChange);
	connect(
		canvas->aclState(), &canvas::AclState::layerAclChanged, this,
		&LayerList::layerLockStatusChanged);
	connect(
		canvas->aclState(), &canvas::AclState::localLockChanged, this,
		&LayerList::userLockStatusChanged);
	connect(
		canvas->aclState(), &canvas::AclState::resetLockChanged, this,
		&LayerList::setDisabled);
	connect(
		m_view->selectionModel(),
		SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this,
		SLOT(selectionChanged(QItemSelection)));
	connect(
		canvas, &canvas::CanvasModel::compatibilityModeChanged, this,
		&LayerList::updateLockedControls);
	connect(
		canvas, &canvas::CanvasModel::compatibilityModeChanged, this,
		&LayerList::updateBlendModes);

	// Init
	m_view->setEnabled(true);
	updateActionLabels();
	updateLockedControls();
	updateCheckActions();
}

static void addLayerButton(
	QWidget *root, QHBoxLayout *layout, QAction *action,
	widgets::GroupedToolButton::GroupPosition position, int stretch)
{
	widgets::GroupedToolButton *button =
		new widgets::GroupedToolButton{position, root};
	button->setDefaultAction(action);
	layout->addWidget(button, stretch);
}

void LayerList::setLayerEditActions(const Actions &actions)
{
	m_actions = actions;

	// Add the actions below the layer list
	QWidget *root = widget();
	Q_ASSERT(root);
	Q_ASSERT(actualTitleBarWidget());

	QHBoxLayout *layout = new QHBoxLayout;
	layout->setSpacing(0);
	layout->setContentsMargins(
		actualTitleBarWidget()->layout()->contentsMargins());
	addLayerButton(
		root, layout, m_actions.addLayer, widgets::GroupedToolButton::GroupLeft,
		3);
	addLayerButton(
		root, layout, m_actions.addGroup,
		widgets::GroupedToolButton::GroupCenter, 2);
	addLayerButton(
		root, layout, m_actions.duplicate,
		widgets::GroupedToolButton::GroupCenter, 2);
	addLayerButton(
		root, layout, m_actions.merge, widgets::GroupedToolButton::GroupCenter,
		2);
	addLayerButton(
		root, layout, m_actions.properties,
		widgets::GroupedToolButton::GroupRight, 2);
	layout->addStretch(3);
	addLayerButton(
		root, layout, m_actions.del, widgets::GroupedToolButton::NotGrouped, 2);
	root->layout()->addItem(layout);

	// Add the actions to the context menu
	m_contextMenu->addAction(m_actions.addLayer);
	m_contextMenu->addAction(m_actions.addGroup);
	m_contextMenu->addAction(m_actions.duplicate);
	m_contextMenu->addAction(m_actions.merge);
	m_contextMenu->addAction(m_actions.del);
	m_contextMenu->addAction(m_actions.properties);
	m_contextMenu->addAction(m_actions.toggleVisibility);
	m_contextMenu->addAction(m_actions.toggleSketch);
	m_contextMenu->addSeparator();
	m_contextMenu->addAction(m_actions.setFillSource);
	m_contextMenu->addAction(m_actions.clearFillSource);
	m_contextMenu->addAction(m_actions.keyFrameSetLayer);

	// Action functionality
	connect(
		m_actions.addLayer, &QAction::triggered, this,
		std::bind(&LayerList::addOrPromptLayerOrGroup, this, false));
	connect(
		m_actions.addGroup, &QAction::triggered, this,
		std::bind(&LayerList::addOrPromptLayerOrGroup, this, true));
	connect(
		m_actions.keyFrameCreateLayer, &QAction::triggered, this,
		std::bind(&LayerList::addLayerOrGroup, this, false, false, true, 0));
	connect(
		m_actions.keyFrameCreateLayerNext, &QAction::triggered, this,
		std::bind(&LayerList::addLayerOrGroup, this, false, false, true, 1));
	connect(
		m_actions.keyFrameCreateLayerPrev, &QAction::triggered, this,
		std::bind(&LayerList::addLayerOrGroup, this, false, false, true, -1));
	connect(
		m_actions.keyFrameCreateGroup, &QAction::triggered, this,
		std::bind(&LayerList::addLayerOrGroup, this, true, false, true, 0));
	connect(
		m_actions.keyFrameCreateGroupNext, &QAction::triggered, this,
		std::bind(&LayerList::addLayerOrGroup, this, true, false, true, 1));
	connect(
		m_actions.keyFrameCreateGroupPrev, &QAction::triggered, this,
		std::bind(&LayerList::addLayerOrGroup, this, true, false, true, -1));
	connect(
		m_actions.keyFrameDuplicateNext, &QAction::triggered, this,
		std::bind(&LayerList::addLayerOrGroup, this, false, true, true, 1));
	connect(
		m_actions.keyFrameDuplicatePrev, &QAction::triggered, this,
		std::bind(&LayerList::addLayerOrGroup, this, false, true, true, -1));
	connect(
		m_actions.duplicate, &QAction::triggered, this,
		&LayerList::duplicateLayer);
	connect(
		m_actions.merge, &QAction::triggered, this, &LayerList::mergeSelected);
	connect(
		m_actions.properties, &QAction::triggered, this,
		&LayerList::showPropertiesOfSelected);
	connect(
		m_actions.toggleVisibility, &QAction::triggered, this,
		&LayerList::toggleLayerVisibility);
	connect(
		m_actions.toggleSketch, &QAction::triggered, this,
		&LayerList::toggleLayerSketch);
	connect(
		m_actions.del, &QAction::triggered, this, &LayerList::deleteSelected);
	connect(
		m_actions.setFillSource, &QAction::triggered, this,
		&LayerList::setFillSourceToSelected);
	connect(
		m_actions.clearFillSource, &QAction::triggered, this,
		&LayerList::clearFillSource);
	connect(
		m_actions.layerCheckToggle, &QAction::triggered, this,
		&LayerList::toggleChecked);
	connect(
		m_actions.layerCheckAll, &QAction::triggered, this,
		&LayerList::checkAll);
	connect(
		m_actions.layerUncheckAll, &QAction::triggered, this,
		&LayerList::uncheckAll);

	updateActionLabels();
	updateLockedControls();
	updateCheckActions();
}

void LayerList::onFeatureAccessChange(DP_Feature feature, bool canUse)
{
	Q_UNUSED(canUse);
	switch(feature) {
	case DP_FEATURE_EDIT_LAYERS:
	case DP_FEATURE_OWN_LAYERS:
		updateLockedControls();
		break;
	default:
		break;
	}
}

void LayerList::updateActionLabels()
{
	QModelIndex idx = currentSelection();
	bool group, check;
	if(idx.isValid()) {
		group = idx.data(canvas::LayerListModel::IsGroupRole).toBool();
		int checkState =
			idx.data(canvas::LayerListModel::CheckStateRole).toInt();
		check = checkState == int(canvas::LayerListModel::Unchecked) ||
				checkState == int(canvas::LayerListModel::NotApplicable) ||
				checkState == int(canvas::LayerListModel::NotCheckable);
	} else {
		group = false;
		check = true;
	}

	if(group) {
		setActionLabel(m_actions.duplicate, tr("Duplicate Layer Group"));
		setActionLabel(m_actions.merge, tr("Merge Layer Group"));
		setActionLabel(m_actions.properties, tr("Layer Group Properties…"));
		setActionLabel(m_actions.del, tr("Delete Layer Group"));
		setActionLabel(
			m_actions.toggleVisibility, tr("Toggle Layer Group &Visibility"));
		setActionLabel(
			m_actions.toggleVisibility, tr("Toggle Layer Group &Sketch Mode"));
		setActionLabel(
			m_actions.layerCheckToggle,
			check ? tr("Check Layer Group") : tr("Uncheck Layer Group"));
	} else {
		setActionLabel(
			m_actions.duplicate,
			QCoreApplication::translate("MainWindow", "Duplicate Layer"));
		setActionLabel(
			m_actions.merge,
			QCoreApplication::translate("MainWindow", "Merge Layer"));
		setActionLabel(
			m_actions.properties,
			QCoreApplication::translate("MainWindow", "Layer Properties…"));
		setActionLabel(
			m_actions.del,
			QCoreApplication::translate("MainWindow", "Delete Layer"));
		setActionLabel(
			m_actions.toggleVisibility,
			QCoreApplication::translate(
				"MainWindow", "Toggle Layer &Visibility"));
		setActionLabel(
			m_actions.toggleSketch,
			QCoreApplication::translate(
				"MainWindow", "Toggle Layer &Sketch Mode"));
		setActionLabel(
			m_actions.layerCheckToggle,
			check ? tr("Check Layer") : tr("Uncheck Layer"));
	}
}

void LayerList::setActionLabel(QAction *action, const QString &text)
{
	if(action) {
		action->setText(text);
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
	const bool hasEditActions = m_actions.addLayer != nullptr;
	if(hasEditActions) {
		m_actions.addLayer->setEnabled(canAdd);
		m_actions.addGroup->setEnabled(canAdd && !compatibilityMode);
		m_actions.layerKeyFrameGroup->setEnabled(canAdd && !compatibilityMode);
	}

	// Rest of the controls need a selection to work.
	const bool enabled =
		!locked && m_selectedId &&
		(canEdit ||
		 (ownLayers && (m_selectedId >> 8) == m_canvas->localUserId()));

	m_lockButton->setEnabled(enabled);
	m_blendModeCombo->setEnabled(enabled);
	m_opacitySlider->setEnabled(m_selectedId != 0 && (m_sketchMode || enabled));
	m_sketchButton->setEnabled(m_selectedId != 0);

	if(hasEditActions) {
		m_actions.duplicate->setEnabled(enabled);
		m_actions.del->setEnabled(enabled);
		m_actions.merge->setEnabled(enabled && canMergeCurrent());
		m_actions.properties->setEnabled(enabled);
		m_actions.toggleVisibility->setEnabled(enabled);
		m_actions.toggleSketch->setEnabled(m_selectedId != 0);
		m_actions.setFillSource->setEnabled(m_selectedId != 0);
	}

	updateFillSourceLayerId();
}

void LayerList::updateBlendModes(bool compatibilityMode)
{
	QModelIndex index = currentSelection();
	if(currentSelection().isValid()) {
		const canvas::LayerListItem &layer =
			index.data().value<canvas::LayerListItem>();
		dialogs::LayerProperties::updateBlendMode(
			m_blendModeCombo, layer.blend, layer.group, layer.isolated,
			compatibilityMode);
	}
}

void LayerList::updateCheckActions()
{
	if(m_actions.layerCheckAll) {
		canvas::LayerListModel *layerlist =
			m_canvas ? m_canvas->layerlist() : nullptr;
		bool checkMode = layerlist && layerlist->isCheckMode();
		bool canToggle = checkMode && layerlist->isLayerCheckStateToggleable(
										  currentSelection());
		m_actions.layerCheckToggle->setEnabled(canToggle);
		m_actions.layerCheckAll->setEnabled(checkMode);
		m_actions.layerUncheckAll->setEnabled(checkMode);
	}
}

void LayerList::selectLayer(int id)
{
	selectLayerIndex(m_canvas->layerlist()->layerIndex(id), true);
}

void LayerList::selectAbove()
{
	disableAutoselectAny();
	selectLayerIndex(m_view->indexAbove(currentSelection()), true);
}

void LayerList::selectBelow()
{
	disableAutoselectAny();
	selectLayerIndex(m_view->indexBelow(currentSelection()), true);
}

void LayerList::setTrackId(int trackId)
{
	m_trackId = trackId;
}

void LayerList::setFrame(int frame)
{
	m_frame = frame;
}

void LayerList::updateFillSourceLayerId()
{
	if(m_canvas) {
		int fillSourceLayerId = m_canvas->layerlist()->fillSourceLayerId();
		m_actions.setFillSource->setEnabled(
			m_selectedId != 0 && m_selectedId != fillSourceLayerId);
		m_actions.clearFillSource->setEnabled(fillSourceLayerId != 0);
	}
}

void LayerList::selectLayerIndex(QModelIndex index, bool scrollTo)
{
	if(index.isValid()) {
		m_view->selectionModel()->select(
			index,
			QItemSelectionModel::SelectCurrent | QItemSelectionModel::Clear);
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
		canvas::LayerListItem layer =
			index.data().value<canvas::LayerListItem>();
		uint8_t flags = ChangeFlags<uint8_t>()
							.set(DP_MSG_LAYER_ATTRIBUTES_FLAGS_CENSOR, censor)
							.update(layer.attributeFlags());
		net::Message msg = net::makeLayerAttributesMessage(
			m_canvas->localUserId(), layer.id, 0, flags, layer.opacity * 255,
			layer.blend);
		emit layerCommands(1, &msg);
	}
}

void LayerList::disableAutoselectAny()
{
	if(m_canvas) {
		m_canvas->layerlist()->setAutoselectAny(false);
	}
}

void LayerList::toggleLayerVisibility()
{
	QModelIndex index = currentSelection();
	if(index.isValid()) {
		canvas::LayerListItem layer =
			index.data().value<canvas::LayerListItem>();
		setLayerVisibility(layer.id, layer.hidden);
	}
}

void LayerList::toggleLayerSketch()
{
	QModelIndex index = currentSelection();
	if(index.isValid()) {
		canvas::LayerListItem layer =
			index.data().value<canvas::LayerListItem>();
		if(layer.sketchOpacity <= 0.0f) {
			const desktop::settings::Settings &settings = dpApp().settings();
			setLayerSketch(
				layer.id, settings.layerSketchOpacityPercent(),
				settings.layerSketchTint());
		} else {
			setLayerSketch(layer.id, 0, QColor());
		}
	}
}

void LayerList::setLayerVisibility(int layerId, bool visible)
{
	m_canvas->paintEngine()->setLayerVisibility(layerId, !visible);
}

void LayerList::setLayerSketch(
	int layerId, int opacityPercent, const QColor &tint)
{
	m_canvas->paintEngine()->setLayerSketch(
		layerId, qRound(opacityPercent / 100.0 * DP_BIT15), tint);
}

void LayerList::changeLayerAcl(
	bool lock, DP_AccessTier tier, QVector<uint8_t> exclusive)
{
	const QModelIndex index = currentSelection();
	if(index.isValid()) {
		uint16_t layerId = index.data(canvas::LayerListModel::IdRole).toInt();
		uint8_t flags = (lock ? DP_ACL_ALL_LOCKED_BIT : 0) | uint8_t(tier);
		net::Message msg = net::makeLayerAclMessage(
			m_canvas->localUserId(), layerId, flags, exclusive);
		emit layerCommands(1, &msg);
	}
}

void LayerList::addOrPromptLayerOrGroup(bool group)
{
	if(dpApp().settings().promptLayerCreate()) {
		showPropertiesForNew(group);
	} else {
		addLayerOrGroup(group, false, false, 0);
	}
}

void LayerList::addLayerOrGroupFromPrompt(
	int selectedId, bool group, const QString &title, int opacityPercent,
	int blendMode, bool isolated, bool censored, bool defaultLayer,
	bool visible, int sketchOpacityPercent, const QColor &sketchTint)
{
	QVector<net::Message> msgs;
	msgs.reserve(2);
	bool selectedExists =
		m_canvas->layerlist()->layerIndex(selectedId).isValid();
	int layerId = makeAddLayerOrGroupCommands(
		msgs, selectedExists ? selectedId : m_selectedId, group, false, false,
		0, title);
	if(layerId > 0 && !msgs.isEmpty()) {
		uint8_t contextId = m_canvas->localUserId();
		if(opacityPercent != 100 || blendMode != DP_BLEND_MODE_NORMAL ||
		   (group && !isolated) || censored) {
			uint8_t flags =
				(group && isolated ? DP_MSG_LAYER_ATTRIBUTES_FLAGS_ISOLATED
								   : 0) |
				(censored ? DP_MSG_LAYER_ATTRIBUTES_FLAGS_CENSOR : 0);
			msgs.append(net::makeLayerAttributesMessage(
				contextId, layerId, 0, flags,
				qRound(opacityPercent / 100.0 * 255), blendMode));
		}
		if(defaultLayer) {
			msgs.append(net::makeDefaultLayerMessage(contextId, layerId));
		}
		emit layerCommands(int(msgs.size()), msgs.constData());
		if(!visible) {
			setLayerVisibility(layerId, false);
		}
		if(sketchOpacityPercent > 0) {
			setLayerSketch(layerId, sketchOpacityPercent, sketchTint);
		}
	}
}

void LayerList::addLayerOrGroup(
	bool group, bool duplicateKeyFrame, bool keyFrame, int keyFrameOffset)
{
	QVector<net::Message> msgs;
	msgs.reserve(4);
	makeAddLayerOrGroupCommands(
		msgs, m_selectedId, group, duplicateKeyFrame, keyFrame, keyFrameOffset,
		QString());
	if(!msgs.isEmpty()) {
		emit layerCommands(int(msgs.size()), msgs.constData());
	}
}

int LayerList::makeAddLayerOrGroupCommands(
	QVector<net::Message> &msgs, int selectedId, bool group,
	bool duplicateKeyFrame, bool keyFrame, int keyFrameOffset,
	const QString &title)
{
	canvas::LayerListModel *layers = m_canvas->layerlist();

	// If we're creating new key frame layers, we intuit the structure from a
	// surrounding key frame if we can find one. That's generally more useful
	// than just creating a plain layer when the track contains other stuff.
	int requiredIdCount = 1;
	QModelIndex referenceIdx;
	if(keyFrame && !group && !duplicateKeyFrame && m_trackId != 0 &&
	   m_frame != -1) {
		referenceIdx = searchKeyFrameReference(requiredIdCount);
		if(referenceIdx.isValid()) {
			group = true;
		}
	}

	QVector<int> ids = layers->getAvailableLayerIds(requiredIdCount);
	if(ids.isEmpty() || int(ids.size()) < requiredIdCount) {
		qWarning("Couldn't find %d free layer id(s)", requiredIdCount);
		return 0;
	}

	int firstId = ids.first();
	uint8_t contextId = m_canvas->localUserId();
	msgs.append(net::makeUndoPointMessage(contextId));

	QModelIndex index = layers->layerIndex(selectedId);
	bool compatibilityMode = m_canvas->isCompatibilityMode();
	if(compatibilityMode) {
		uint16_t targetId = index.isValid() ? selectedId : 0;
		uint8_t flags = targetId == 0 ? 0 : DP_MSG_LAYER_CREATE_FLAGS_INSERT;
		msgs.append(net::makeLayerCreateMessage(
			contextId, firstId, targetId, 0, flags,
			title.isEmpty() ? layers->getAvailableLayerName(getBaseName(false))
							: title));
	} else {
		int targetId = -1;
		int sourceId = 0;
		int targetFrame = -1;
		int moveId = -1;
		uint8_t flags = group ? DP_MSG_LAYER_TREE_CREATE_FLAGS_GROUP : 0;

		if(keyFrame && m_trackId != 0 && m_frame != -1) {
			// TODO: having to do a layer move here is dumb, there should be a
			// layer create flag that throws the layer at the bottom instead.
			targetFrame = m_frame + keyFrameOffset;
			moveId = intuitKeyFrameTarget(
				duplicateKeyFrame ? m_frame : -1, targetFrame, sourceId,
				targetId, flags);
		}

		if(targetId == -1 && index.isValid()) {
			targetId = selectedId;
			bool into =
				index.data(canvas::LayerListModel::IsGroupRole).toBool() &&
				(m_view->isExpanded(index) ||
				 index.data(canvas::LayerListModel::IsEmptyRole).toBool());
			if(into) {
				flags |= DP_MSG_LAYER_TREE_CREATE_FLAGS_INTO;
			}
		}

		QString effectiveTitle;
		if(title.isEmpty()) {
			QString baseName;
			if(sourceId != 0) {
				QModelIndex sourceIndex = layers->layerIndex(sourceId);
				if(sourceIndex.isValid()) {
					baseName =
						sourceIndex.data(canvas::LayerListModel::TitleRole)
							.toString();
				}
			}
			effectiveTitle = layers->getAvailableLayerName(
				baseName.isEmpty() ? getBaseName(group) : baseName);
		} else {
			effectiveTitle = title;
		}

		msgs.append(net::makeLayerTreeCreateMessage(
			contextId, firstId, sourceId, qMax(0, targetId), 0, flags,
			effectiveTitle));
		if(targetFrame >= 0) {
			msgs.append(net::makeKeyFrameSetMessage(
				contextId, m_trackId, targetFrame, firstId, 0,
				DP_MSG_KEY_FRAME_SET_SOURCE_LAYER));
		}
		if(moveId != -1) {
			msgs.append(net::makeLayerTreeMoveMessage(
				contextId, firstId, targetId, moveId));
		}

		if(referenceIdx.isValid()) {
			makeKeyFrameReferenceEditCommands(
				msgs, contextId, referenceIdx, firstId);
			int idIndex = 1;
			makeKeyFrameReferenceAddCommands(
				layers, msgs, ids, idIndex, contextId, referenceIdx, firstId);
		}
	}

	layers->setLayerIdToSelect(firstId);
	return firstId;
}

QModelIndex LayerList::searchKeyFrameReference(int &outRequiredIdCount) const
{
	const canvas::TimelineModel *timeline = m_canvas->timeline();
	const canvas::TimelineTrack *track = timeline->getTrackById(m_trackId);
	if(!track) {
		return QModelIndex();
	}

	int bestFrameIndex = -1;
	QModelIndex idx;
	const canvas::LayerListModel *layerlist = m_canvas->layerlist();
	for(const canvas::TimelineKeyFrame &keyFrame : track->keyFrames) {
		if(keyFrame.layerId == 0) {
			continue;
		}

		QModelIndex candidate = layerlist->layerIndex(keyFrame.layerId);
		if(!candidate.isValid()) {
			continue;
		}

		if(idx.isValid()) {
			if(bestFrameIndex <= m_frame) {
				if(keyFrame.frameIndex > m_frame ||
				   keyFrame.frameIndex < bestFrameIndex) {
					continue;
				}
			} else if(keyFrame.frameIndex > bestFrameIndex) {
				continue;
			}
		}

		bestFrameIndex = keyFrame.frameIndex;
		idx = candidate;
	}

	if(!idx.isValid() ||
	   !idx.data(canvas::LayerListModel::IsGroupRole).toBool()) {
		return QModelIndex();
	}

	outRequiredIdCount = countRequiredIds(layerlist, idx);
	return idx;
}

int LayerList::countRequiredIds(
	const canvas::LayerListModel *layerlist, const QModelIndex &idx)
{
	if(idx.isValid()) {
		int count = 1;
		int childCount = layerlist->rowCount(idx);
		for(int i = 0; i < childCount; ++i) {
			count += countRequiredIds(layerlist, layerlist->index(i, 0, idx));
		}
		return count;
	} else {
		return 0;
	}
}

int LayerList::intuitKeyFrameTarget(
	int sourceFrame, int targetFrame, int &sourceId, int &targetId,
	uint8_t &flags) const
{
	// Guess where we're supposed to throw this new layer in relation to
	// surrounding frames in the timeline. If there's a previous key frame, we
	// put it above its layer. If not, put it below the previous key frame's
	// layer. In absence of both, we just act like it's a regular layer.
	const canvas::TimelineModel *timeline = m_canvas->timeline();
	const canvas::TimelineTrack *track = timeline->getTrackById(m_trackId);
	if(!track) {
		return -1;
	}

	const canvas::TimelineKeyFrame *keyFrameBefore = nullptr;
	const canvas::TimelineKeyFrame *keyFrameAfter = nullptr;
	for(const canvas::TimelineKeyFrame &keyFrame : track->keyFrames) {
		if(keyFrame.layerId != 0) {
			int frame = keyFrame.frameIndex;
			if(frame == sourceFrame) {
				sourceId = keyFrame.layerId;
			}

			if(frame <= targetFrame) {
				if(!keyFrameBefore || frame > keyFrameBefore->frameIndex) {
					keyFrameBefore = &keyFrame;
				}
			} else {
				if(!keyFrameAfter || frame < keyFrameAfter->frameIndex) {
					keyFrameAfter = &keyFrame;
				}
			}
		}
	}

	if(keyFrameBefore) {
		targetId = keyFrameBefore->layerId;
		return -1;
	} else if(keyFrameAfter) {
		const canvas::LayerListModel *layers = m_canvas->layerlist();
		QModelIndex index = layers->layerIndex(keyFrameAfter->layerId);
		if(!index.isValid()) {
			return -1;
		}

		QModelIndex sibling = index.siblingAtRow(index.row() + 1);
		if(sibling.isValid()) {
			targetId = sibling.data(canvas::LayerListModel::IdRole).toInt();
			return -1;
		} else {
			QModelIndex parent = index.parent();
			if(parent.isValid()) {
				targetId = parent.data(canvas::LayerListModel::IdRole).toInt();
				flags |= DP_MSG_LAYER_TREE_CREATE_FLAGS_INTO;
			} else {
				targetId = 0;
			}
			return index.data(canvas::LayerListModel::IdRole).toInt();
		}
	} else {
		return -1;
	}
}

void LayerList::makeKeyFrameReferenceAddCommands(
	const canvas::LayerListModel *layerlist, QVector<net::Message> &msgs,
	QVector<int> ids, int &idIndex, uint8_t contextId,
	const QModelIndex &parent, int parentId) const
{
	int childCount = layerlist->rowCount(parent);
	for(int i = 0; i < childCount; ++i) {
		QModelIndex idx = layerlist->index(childCount - i - 1, 0, parent);
		if(idx.isValid() && idIndex < ids.size()) {
			int id = ids[idIndex++];
			bool group = idx.data(canvas::LayerListModel::IsGroupRole).toBool();
			uint8_t flags = DP_MSG_LAYER_TREE_CREATE_FLAGS_INTO |
							(group ? DP_MSG_LAYER_TREE_CREATE_FLAGS_GROUP : 0);
			msgs.append(net::makeLayerTreeCreateMessage(
				contextId, id, 0, parentId, 0, flags,
				idx.data(canvas::LayerListModel::TitleRole).toString()));
			makeKeyFrameReferenceEditCommands(msgs, contextId, idx, id);
			if(group) {
				makeKeyFrameReferenceAddCommands(
					layerlist, msgs, ids, idIndex, contextId, idx, id);
			}
		}
	}
}

void LayerList::makeKeyFrameReferenceEditCommands(
	QVector<net::Message> &msgs, uint8_t contextId, const QModelIndex &idx,
	int id) const
{
	const canvas::LayerListItem &layer =
		idx.data().value<canvas::LayerListItem>();
	if(layer.opacity != 1.0f || layer.blend != DP_BLEND_MODE_NORMAL ||
	   layer.censored || (layer.group && !layer.isolated)) {
		uint8_t flags =
			(layer.censored ? DP_MSG_LAYER_ATTRIBUTES_FLAGS_CENSOR : 0) |
			(layer.group && layer.isolated
				 ? DP_MSG_LAYER_ATTRIBUTES_FLAGS_ISOLATED
				 : 0);
		msgs.append(net::makeLayerAttributesMessage(
			contextId, id, 0, flags, uint8_t(layer.opacity * 255.0f),
			uint8_t(layer.blend)));
	}
}

void LayerList::duplicateLayer()
{
	const QModelIndex index = currentSelection();
	const canvas::LayerListItem layer =
		index.data().value<canvas::LayerListItem>();

	canvas::LayerListModel *layers = m_canvas->layerlist();
	Q_ASSERT(layers);

	const int id = layers->getAvailableLayerId();
	if(id == 0) {
		qWarning("Couldn't find a free ID for duplicating layer!");
		return;
	}

	uint8_t contextId = m_canvas->localUserId();

	net::Message msg;
	if(m_canvas->isCompatibilityMode()) {
		msg = net::makeLayerCreateMessage(
			contextId, id, layer.id, 0,
			DP_MSG_LAYER_CREATE_FLAGS_COPY | DP_MSG_LAYER_CREATE_FLAGS_INSERT,
			layers->getAvailableLayerName(layer.title));
	} else {
		msg = net::makeLayerTreeCreateMessage(
			contextId, id, layer.id, layer.id, 0, 0,
			layers->getAvailableLayerName(layer.title));
	}

	layers->setLayerIdToSelect(id);
	net::Message messages[] = {net::makeUndoPointMessage(contextId), msg};
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
		const QModelIndex below = index.sibling(index.row() + 1, 0);
		return below.isValid() &&
			   !below.data(canvas::LayerListModel::IsGroupRole).toBool() &&
			   !m_canvas->aclState()->isLayerLocked(
				   below.data(canvas::LayerListModel::IdRole).toInt());
	}
}

void LayerList::deleteSelected()
{
	QModelIndex index = currentSelection();
	if(!index.isValid())
		return;

	const canvas::LayerListItem &layer =
		index.data().value<canvas::LayerListItem>();
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
	net::Message msg;
	if(m_canvas->isCompatibilityMode()) {
		msg = net::makeLayerDeleteMessage(
			contextId, index.data().value<canvas::LayerListItem>().id, false);
	} else {
		msg = net::makeLayerTreeDeleteMessage(
			contextId, index.data().value<canvas::LayerListItem>().id, 0);
	}

	net::Message messages[] = {net::makeUndoPointMessage(contextId), msg};
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
	net::Message msg;
	if(m_canvas->isCompatibilityMode()) {
		QModelIndex below = index.sibling(index.row() + 1, 0);
		if(!below.isValid()) {
			return;
		}
		msg = net::makeLayerDeleteMessage(contextId, layerId, true);
	} else {
		int mergeId;
		if(index.data(canvas::LayerListModel::IsGroupRole).toBool()) {
			mergeId = layerId;
		} else {
			QModelIndex below = index.sibling(index.row() + 1, 0);
			if(!below.isValid()) {
				return;
			}
			mergeId = below.data(canvas::LayerListModel::IdRole).toInt();
		}
		msg = net::makeLayerTreeDeleteMessage(contextId, layerId, mergeId);
	}

	net::Message messages[] = {net::makeUndoPointMessage(contextId), msg};
	emit layerCommands(DP_ARRAY_LENGTH(messages), messages);
}

void LayerList::setFillSourceToSelected()
{
	QModelIndex index = currentSelection();
	if(index.isValid()) {
		emit fillSourceSet(index.data(canvas::LayerListModel::IdRole).toInt());
	}
}

void LayerList::clearFillSource()
{
	emit fillSourceSet(0);
}

void LayerList::toggleChecked()
{
	if(m_canvas) {
		QModelIndex idx = currentSelection();
		canvas::LayerListModel *layerlist = m_canvas->layerlist();
		if(layerlist->isLayerCheckStateToggleable(idx)) {
			emit layerChecked(
				idx.data(canvas::LayerListModel::IdRole).toInt(),
				idx.data(canvas::LayerListModel::CheckStateRole).toInt() ==
					int(canvas::LayerListModel::Unchecked));
		}
	}
}

void LayerList::checkAll()
{
	if(m_canvas) {
		m_canvas->layerlist()->setAllChecked(true);
	}
}

void LayerList::uncheckAll()
{
	if(m_canvas) {
		m_canvas->layerlist()->setAllChecked(false);
	}
}

void LayerList::showPropertiesForNew(bool group)
{
	QString dialogObjectName = QStringLiteral("layerpropertiesnewlayer");
	dialogs::LayerProperties *dlg = findChild<dialogs::LayerProperties *>(
		dialogObjectName, Qt::FindDirectChildrenOnly);
	if(!dlg) {
		dlg = makeLayerPropertiesDialog(dialogObjectName, QModelIndex());
		dlg->setNewLayerItem(
			m_selectedId, group,
			m_canvas->layerlist()->getAvailableLayerName(getBaseName(group)));
	}
	dlg->show();
	dlg->activateWindow();
	dlg->raise();
}

void LayerList::showPropertiesOfSelected()
{
	showPropertiesOfIndex(currentSelection());
}

void LayerList::showPropertiesOfIndex(QModelIndex index)
{
	if(index.isValid()) {
		int layerId = index.data(canvas::LayerListModel::IdRole).toInt();
		QString dialogObjectName =
			QStringLiteral("layerproperties%1").arg(layerId);
		dialogs::LayerProperties *dlg = findChild<dialogs::LayerProperties *>(
			dialogObjectName, Qt::FindDirectChildrenOnly);
		if(!dlg) {
			dlg = makeLayerPropertiesDialog(dialogObjectName, index);
			dlg->setLayerItem(
				index.data().value<canvas::LayerListItem>(),
				layerCreatorName(layerId),
				index.data(canvas::LayerListModel::IsDefaultRole).toBool());
		}
		dlg->show();
		dlg->activateWindow();
		dlg->raise();
	}
}

dialogs::LayerProperties *LayerList::makeLayerPropertiesDialog(
	const QString &dialogObjectName, const QModelIndex &index)
{
	dialogs::LayerProperties *dlg =
		new dialogs::LayerProperties(m_canvas->localUserId(), this);
	dlg->setObjectName(dialogObjectName);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setModal(false);

	connect(
		m_canvas, &canvas::CanvasModel::compatibilityModeChanged, dlg,
		&dialogs::LayerProperties::setCompatibilityMode);
	connect(
		dlg, &dialogs::LayerProperties::layerCommands, this,
		&LayerList::layerCommands);

	bool isOwnLayer;
	if(index.isValid()) {
		int layerId = index.data(canvas::LayerListModel::IdRole).toInt();
		isOwnLayer = (layerId & 0xff00) >> 8 == m_canvas->localUserId();
		connect(
			dlg, &dialogs::LayerProperties::visibilityChanged, this,
			&LayerList::setLayerVisibility);
		connect(
			dlg, &dialogs::LayerProperties::sketchModeChanged, this,
			&LayerList::setLayerSketch);
		connect(
			m_canvas->layerlist(), &canvas::LayerListModel::modelReset, dlg,
			[this, dlg]() {
				QModelIndex newIndex =
					m_canvas->layerlist()->layerIndex(dlg->layerId());
				if(newIndex.isValid()) {
					dlg->updateLayerItem(
						newIndex.data().value<canvas::LayerListItem>(),
						layerCreatorName(dlg->layerId()),
						newIndex.data(canvas::LayerListModel::IsDefaultRole)
							.toBool());
				} else {
					dlg->deleteLater();
				}
			});
	} else {
		isOwnLayer = true;
		connect(
			dlg, &dialogs::LayerProperties::addLayerOrGroupRequested, this,
			&LayerList::addLayerOrGroupFromPrompt);
	}

	bool canEditAll =
		m_canvas->aclState()->canUseFeature(DP_FEATURE_EDIT_LAYERS);
	bool canEdit =
		canEditAll ||
		(m_canvas->aclState()->canUseFeature(DP_FEATURE_OWN_LAYERS) &&
		 isOwnLayer);
	dlg->setControlsEnabled(canEdit);
	dlg->setOpControlsEnabled(canEditAll);
	dlg->setCompatibilityMode(m_canvas->isCompatibilityMode());

	return dlg;
}

void LayerList::showSketchTintColorPicker()
{
	if(m_sketchMode) {
		QModelIndex index = currentSelection();
		if(index.isValid()) {
			canvas::LayerListItem layer =
				index.data().value<canvas::LayerListItem>();
			if(layer.sketchOpacity > 0.0f) {
				color_widgets::ColorDialog *dlg =
					dialogs::newDeleteOnCloseColorDialog(
						layer.sketchTint, this);
				dlg->setAlphaEnabled(true);
				dlg->setPreviewDisplayMode(
					color_widgets::ColorPreview::AllAlpha);
				connect(
					dlg, &color_widgets::ColorDialog::colorSelected, this,
					&LayerList::setUpdateSketchTint);
				dlg->show();
			}
		}
	}
}

void LayerList::setUpdateSketchTint(const QColor &tint)
{
	m_updateSketchTint = tint;
	triggerUpdate();
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
	m_nearestToDeletedId =
		m_canvas->layerlist()->findNearestLayer(m_selectedId);
	m_lastScrollPosition = m_view->verticalScrollBar()->value();
	m_expandedGroups.clear();
	for(const canvas::LayerListItem &item :
		m_canvas->layerlist()->layerItems()) {
		if(m_view->isExpanded(m_canvas->layerlist()->layerIndex(item.id))) {
			m_expandedGroups.insert(item.id);
		}
	}
}

void LayerList::afterLayerReset()
{
	const bool wasAnimated = m_view->isAnimated();
	m_view->setAnimated(false);

	for(int layerId : m_expandedGroups) {
		QModelIndex index = m_canvas->layerlist()->layerIndex(layerId);
		if(index.isValid()) {
			m_view->setExpanded(index, true);
		}
	}

	if(m_canvas->layerlist()->rowCount() == 0) {
		// If there's nothing to select, the selection model doesn't emit
		// a selection change signal, so we have to call this manually.
		selectionChanged(QItemSelection{});
	} else if(m_selectedId) {
		const auto selectedIndex =
			m_canvas->layerlist()->layerIndex(m_selectedId);
		if(selectedIndex.isValid()) {
			selectLayerIndex(selectedIndex);
		} else {
			selectLayer(m_nearestToDeletedId);
		}
	}

	m_view->verticalScrollBar()->setValue(m_lastScrollPosition);
	m_view->setAnimated(wasAnimated);
	updateCheckActions();
}

bool LayerList::isGroupSelected() const
{
	QModelIndex idx = currentSelection();
	return idx.isValid() &&
		   idx.data(canvas::LayerListModel::IsGroupRole).toBool();
}

QModelIndex LayerList::currentSelection() const
{
	QItemSelectionModel *selectionModel = m_view->selectionModel();
	if(selectionModel) {
		QModelIndexList sel = selectionModel->selectedIndexes();
		if(!sel.isEmpty()) {
			return sel.first();
		}
	}
	return QModelIndex();
}

QFlags<view::Lock::Reason> LayerList::currentLayerLock() const
{
	using Reason = view::Lock::Reason;
	QFlags<Reason> reasons = Reason::None;
	if(m_canvas) {
		QModelIndex idx = currentSelection();
		if(idx.isValid()) {
			const canvas::LayerListItem &item =
				idx.data().value<canvas::LayerListItem>();
			if(idx.data(canvas::LayerListModel::IsHiddenInTreeRole).toBool()) {
				reasons.setFlag(Reason::LayerHidden);
			}
			if(item.group) {
				reasons.setFlag(Reason::LayerGroup);
			}
			if(idx.data(canvas::LayerListModel::IsLockedRole).toBool()) {
				reasons.setFlag(Reason::LayerLocked);
			}
			if(idx.data(canvas::LayerListModel::IsHiddenInFrameRole).toBool()) {
				reasons.setFlag(Reason::LayerHiddenInFrame);
			}
			if(idx.data(canvas::LayerListModel::IsCensoredInTreeRole)
				   .toBool()) {
				reasons.setFlag(Reason::LayerCensored);
			}
		}
	}
	return reasons;
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

	updateActionLabels();
	updateLockedControls();
	updateCheckActions();

	emit layerSelected(m_selectedId);
}

void LayerList::updateUiFromSelection()
{
	const canvas::LayerListItem &layer =
		currentSelection().data().value<canvas::LayerListItem>();
	m_noupdate = true;
	m_selectedId = layer.id;

	m_sketchMode = layer.sketchOpacity > 0.0f;
	m_sketchButton->setChecked(m_sketchMode);
	m_sketchButton->setGroupPosition(
		m_sketchMode ? widgets::GroupedToolButton::GroupLeft
					 : widgets::GroupedToolButton::NotGrouped);
	m_sketchTintButton->setEnabled(m_sketchMode);
	m_sketchTintButton->setVisible(m_sketchMode);
	if(m_sketchMode) {
		m_sketchTintButton->setIcon(
			utils::makeColorIconFor(this, layer.sketchTint));
	}

	m_aclmenu->setCensored(layer.actuallyCensored());
	dialogs::LayerProperties::updateBlendMode(
		m_blendModeCombo, layer.blend, layer.group, layer.isolated,
		m_canvas->isCompatibilityMode());
	m_opacitySlider->setPrefix(m_sketchMode ? tr("Sketch: ") : tr("Opacity: "));
	m_opacitySlider->setValue(
		(m_sketchMode ? layer.sketchOpacity : layer.opacity) * 100.0 + 0.5);
	m_opacitySlider->setMinimum(m_sketchMode ? 1 : 0);

	layerLockStatusChanged(layer.id);
	updateActionLabels();
	updateLockedControls();
	updateCheckActions();

	// TODO use change flags to detect if this really changed
	emit activeLayerVisibilityChanged();
	m_noupdate = false;
}

void LayerList::layerLockStatusChanged(int layerId)
{
	if(m_selectedId == layerId) {
		const auto acl = m_canvas->aclState()->layerAcl(layerId);
		m_lockButton->setChecked(
			acl.locked || acl.tier != DP_ACCESS_TIER_GUEST ||
			!acl.exclusive.isEmpty());
		m_aclmenu->setAcl(acl.locked, int(acl.tier), acl.exclusive);

		emit activeLayerVisibilityChanged();
	}
}

void LayerList::userLockStatusChanged(bool)
{
	updateLockedControls();
	updateCheckActions();
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
	if(m_selectedId != 0 && !m_noupdate) {
		if(m_sketchMode) {
			m_updateSketchOpacity = value;
		} else {
			m_updateOpacity = value;
		}
		m_debounceTimer->start();
	}
}

void LayerList::triggerUpdate()
{
	m_debounceTimer->stop();
	QModelIndex index = currentSelection();
	if(index.isValid()) {
		const canvas::LayerListItem &layer =
			index.data().value<canvas::LayerListItem>();

		if(m_updateBlendModeIndex != -1 || m_updateOpacity != -1) {
			DP_BlendMode mode;
			bool isolated;
			if(m_updateBlendModeIndex == -1) {
				mode = layer.blend;
				isolated = layer.isolated;
			} else {
				int blendModeData =
					m_blendModeCombo->itemData(m_updateBlendModeIndex).toInt();
				mode = blendModeData == -1 ? layer.blend
										   : DP_BlendMode(blendModeData);
				isolated = layer.group && blendModeData != -1;
			}

			float opacity;
			if(m_updateOpacity == -1) {
				opacity = layer.opacity;
			} else {
				opacity = m_updateOpacity / 100.0f;
			}

			uint8_t flags =
				(layer.actuallyCensored() ? DP_MSG_LAYER_ATTRIBUTES_FLAGS_CENSOR
										  : 0) |
				(isolated ? DP_MSG_LAYER_ATTRIBUTES_FLAGS_ISOLATED : 0);

			net::Message msg = net::makeLayerAttributesMessage(
				m_canvas->localUserId(), layer.id, 0, flags,
				opacity * 255.0f + 0.5f, mode);

			emit layerCommands(1, &msg);
		}

		if(m_updateSketchOpacity != -1 || m_updateSketchTint.isValid()) {
			if(m_sketchMode) {
				desktop::settings::Settings &settings = dpApp().settings();

				int sketchOpacity;
				if(m_updateSketchOpacity != -1) {
					sketchOpacity = m_updateSketchOpacity;
					settings.setLayerSketchOpacityPercent(sketchOpacity);
				} else {
					sketchOpacity = layer.sketchOpacity * 100.0 + 0.5;
				}

				QColor sketchTint;
				if(m_updateSketchTint.isValid()) {
					sketchTint = m_updateSketchTint;
					settings.setLayerSketchTint(sketchTint);
				} else {
					sketchTint = layer.sketchTint;
				}

				setLayerSketch(layer.id, sketchOpacity, sketchTint);
			}
		}
	}
	m_updateBlendModeIndex = -1;
	m_updateOpacity = -1;
	m_updateSketchOpacity = -1;
	m_updateSketchTint = QColor();
}

QString LayerList::getBaseName(bool group)
{
	if(m_canvas) {
		QString username = m_canvas->userlist()
							   ->getUserById(m_canvas->localUserId())
							   .name.trimmed();
		if(!username.isEmpty()) {
			return username;
		}
	}
	return group ? tr("Group") : tr("Layer");
}

}
