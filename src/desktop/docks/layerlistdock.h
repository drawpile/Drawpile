// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DOCKS_LAYERLISTDOCK_H
#define DESKTOP_DOCKS_LAYERLISTDOCK_H
#include "desktop/docks/dockbase.h"
#include "desktop/view/lock.h"
#include "libclient/canvas/acl.h"
#include "libclient/net/message.h"
#include <QColor>
#include <QModelIndexList>
#include <QScroller>
#include <QSet>
#include <QVector>
#include <functional>

class KisSliderSpinBox;
class QAction;
class QActionGroup;
class QComboBox;
class QItemSelection;
class QMenu;
class QModelIndex;
class QSlider;
class QTimer;
class QTreeView;

namespace canvas {
class CanvasModel;
class LayerListModel;
}

namespace dialogs {
class LayerProperties;
}

namespace widgets {
class GroupedToolButton;
}

namespace docks {

class LayerAclMenu;

class LayerList final : public DockBase {
	Q_OBJECT
public:
	struct Actions {
		QAction *addLayer = nullptr;
		QAction *addGroup = nullptr;
		QAction *duplicate = nullptr;
		QAction *merge = nullptr;
		QAction *properties = nullptr;
		QAction *del = nullptr;
		QAction *toggleVisibility = nullptr;
		QAction *toggleSketch = nullptr;
		QAction *setFillSource = nullptr;
		QAction *clearFillSource = nullptr;
		QAction *keyFrameSetLayer = nullptr;
		QAction *keyFrameCreateLayer = nullptr;
		QAction *keyFrameCreateLayerNext = nullptr;
		QAction *keyFrameCreateLayerPrev = nullptr;
		QAction *keyFrameCreateGroup = nullptr;
		QAction *keyFrameCreateGroupNext = nullptr;
		QAction *keyFrameCreateGroupPrev = nullptr;
		QAction *keyFrameDuplicateNext = nullptr;
		QAction *keyFrameDuplicatePrev = nullptr;
		QActionGroup *layerKeyFrameGroup = nullptr;
		QAction *layerCheckToggle = nullptr;
		QAction *layerCheckAll = nullptr;
		QAction *layerUncheckAll = nullptr;
		QActionGroup *layerAlphaGroup = nullptr;
		QAction *layerAlphaBlend = nullptr;
		QAction *layerAlphaPreserve = nullptr;
		QAction *layerClip = nullptr;
		QAction *layerAutomaticAlphaPreserve = nullptr;
		QMenu *layerColorMenu = nullptr;
		QMenu *layerLockMenu = nullptr;
		QAction *layerAlphaLock = nullptr;
		QAction *layerLockAll = nullptr;
		QAction *layerLockContent = nullptr;
		QAction *layerLockProps = nullptr;
		QAction *layerLockMove = nullptr;
		QAction *layerCensor = nullptr;
	};

	LayerList(QWidget *parent = nullptr);

	void setCanvas(canvas::CanvasModel *canvas);

	//! These actions are shown in a menu outside this dock
	void setLayerEditActions(const Actions &actions);

	/**
	 * Is the currently selected layer locked for editing?
	 *
	 * This may be because it is actually locked or because it is hidden or a
	 * non-editable (group) layer.
	 */
	QFlags<view::Lock::Reason> currentLayerLock() const;

	int currentId() const { return m_currentId; }

	bool isExpanded(const QModelIndex &index) const;
	bool isSelected(int id) const { return m_selectedIds.contains(id); }

	void updateLayerColorMenuIcon();

	void selectLayer(int id);
	void selectAbove();
	void selectBelow();

	void setTrackId(int trackId);
	void setFrame(int frame);
	void updateFillSourceLayerId();

signals:
	//! A layer was selected by the user
	void layerSelected(int id);
	void layerSelectionChanged(const QSet<int> &layerIds);
	void layerChecked(int layerId, bool checked);
	void activeLayerVisibilityChanged();
	void fillSourceSet(int layerId);

	void layerCommands(int count, const net::Message *msgs);

private:
	void beforeLayerReset();
	void afterLayerReset();

	void onFeatureAccessChange(DP_Feature feature, bool canuse);

	void duplicateLayer();
	void deleteSelected();
	void mergeSelected();
	void setFillSourceToCurrent();
	void clearFillSource();
	void toggleChecked();
	void checkAll();
	void uncheckAll();

	void showContextMenu(const QPoint &pos);
	void changeLayersAlphaLock(bool alphaLock);
	void changeLayersContentLock(bool contentLock);
	void changeLayersPropsLock(bool propsLock);
	void changeLayersMoveLock(bool moveLock);
	void changeLayersLockAll(bool lockAll);
	void changeLayersAccessTier(int tier);
	void changeLayersUserAccess(int userId, bool access);
	void changeLayersAclWith(
		const std::function<bool(canvas::AclState::Layer &)> &fn);
	void changeLayersCensor(bool censor);
	void disableAutoselectAny();
	void toggleLayerVisibility();
	void toggleLayerSketch();
	void setLayerVisibility(int layerId, bool visible);
	void setLayerSketch(int layerId, int opacityPercent, const QColor &tint);
	void toggleSelection(const QModelIndex &idx);
	void
	changeLayerAcl(bool lock, DP_AccessTier tier, QVector<uint8_t> exclusive);

	void layerLockStatusChanged(int layerId);
	void userLockStatusChanged(bool);
	void clipChanged(bool clip);
	void alphaPreserveChanged(bool alphaPreserve);
	void blendModeChanged(int index);
	void opacityChanged(int value);

	void triggerUpdate();

	void updateActionLabels();
	void setActionLabel(QAction *action, const QString &text);
	void updateLockedControls();
	void updateCheckActions();
	void forceRefreshMargin();
	QModelIndex canMerge(const QSet<int> &topLevelIds) const;
	bool canEditLayer(const QModelIndex &idx) const;

	void
	currentChanged(const QModelIndex &current, const QModelIndex &previous);
	void selectionChanged(
		const QItemSelection &selected, const QItemSelection &deselected);
	void updateCurrent(const QModelIndex &current);
	void updateUiFromCurrent();
	void setAutomaticAlphaPreserve(bool automaticAlphaPreserve);

	void addOrPromptLayerOrGroup(bool group);
	void addLayerOrGroupFromPrompt(
		int selectedId, bool group, const QString &title, int opacityPercent,
		int blendMode, bool isolated, bool censored, bool clip,
		bool defaultLayer, bool visible, int sketchOpacityPercent,
		const QColor &sketchTint);
	void addLayerOrGroup(
		bool group, bool duplicateKeyFrame, bool keyFrame, int keyFrameOffset);
	int makeAddLayerOrGroupCommands(
		net::MessageList &msgs, int selectedId, bool group,
		bool duplicateKeyFrame, bool keyFrame, int keyFrameOffset,
		const QString &title);
	QModelIndex searchKeyFrameReference(int &outRequiredIdCount) const;
	static int countRequiredIds(
		const canvas::LayerListModel *layerlist, const QModelIndex &idx);
	int intuitKeyFrameTarget(
		int sourceFrame, int targetFrame, int &sourceId, int &targetId,
		uint8_t &flags) const;
	void makeKeyFrameReferenceAddCommands(
		const canvas::LayerListModel *layerlist, net::MessageList &msgs,
		QVector<int> ids, int &idIndex, uint8_t contextId,
		const QModelIndex &parent, int parentId) const;
	void makeKeyFrameReferenceEditCommands(
		net::MessageList &msgs, uint8_t contextId, const QModelIndex &idx,
		int id) const;
	void doDeleteSelected();

	void setLayerColor(QAction *action);

	void showOutOfIdsError(int layerIdLimit, int requiredIdCount);

	void showPropertiesForNew(bool group);
	void showPropertiesOfCurrent();
	void showPropertiesOfIndex(const QModelIndex &index);
	dialogs::LayerProperties *makeLayerPropertiesDialog(
		const QString &dialogObjectName, const QModelIndex &index);

	void showSketchTintColorPicker();
	void setUpdateSketchTint(const QColor &tint);

	void setLayerAlphaBlend();
	void setLayerAlphaPreserve();
	void setLayerClip();

	bool isGroupSelected() const;
	QModelIndex currentSelection() const;
	bool ownsAllTopLevelSelections() const;
	bool ownsAll(const QSet<int> &layerIds) const;
	QModelIndexList topLevelSelections() const;
	QSet<int> topLevelSelectedIds() const;
	void
	gatherTopLevel(const std::function<void(const QModelIndex &)> &fn) const;
	void selectLayerIndex(QModelIndex index, bool scrollTo = false);

	QString layerCreatorName(int layerId) const;

	QString getBaseName(bool group);

	canvas::CanvasModel *m_canvas = nullptr;

	// cache selection and remember it across model resets
	int m_currentId = 0;
	int m_nearestToDeletedId = 0;
	QSet<int> m_selectedIds;

	// try to retain view status across model resets
	QSet<int> m_expandedGroups;
	int m_lastScrollPosition = 0;

	int m_trackId = 0;
	int m_frame = -1;

	bool m_noupdate = false;
	bool m_sketchMode = false;
	bool m_automaticAlphaPreserve = true;

	QMenu *m_contextMenu;
	LayerAclMenu *m_aclmenu = nullptr;

	QTimer *m_debounceTimer;
	int m_updateClip = -1;
	int m_updateAlphaPreserve = -1;
	int m_updateBlendModeIndex = -1;
	int m_updateOpacity = -1;
	int m_updateSketchOpacity = -1;
	QColor m_updateSketchTint;

	widgets::GroupedToolButton *m_lockButton;
	widgets::GroupedToolButton *m_clipButton;
	widgets::GroupedToolButton *m_alphaPreserveButton;
	QComboBox *m_blendModeCombo;
	KisSliderSpinBox *m_opacitySlider;
	widgets::GroupedToolButton *m_sketchButton;
	widgets::GroupedToolButton *m_sketchTintButton;
	QTreeView *m_view;

	Actions m_actions;
};

#ifdef DRAWPILE_QSCROLLER_PATCH
class LayerListScrollFilter : public DrawpileQScrollerFilter {
	Q_OBJECT
public:
	explicit LayerListScrollFilter(LayerList *parent);

	bool filterScroll(QWidget *w, const QPointF &point) override;
};
#endif

}

#endif
