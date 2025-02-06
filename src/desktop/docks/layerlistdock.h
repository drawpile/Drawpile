// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LAYERLISTDOCK_H
#define LAYERLISTDOCK_H
extern "C" {
#include <dpmsg/acl.h>
}
#include "desktop/docks/dockbase.h"
#include "desktop/view/lock.h"
#include "libclient/net/message.h"
#include <QColor>
#include <QModelIndexList>
#include <QSet>
#include <QVector>

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

public slots:
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

private slots:
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
	void censorSelected(bool censor);
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
	void blendModeChanged(int index);
	void opacityChanged(int value);

	void triggerUpdate();

private:
	void updateActionLabels();
	void setActionLabel(QAction *action, const QString &text);
	void updateLockedControls();
	void updateBlendModes(bool compatibilityMode);
	void updateCheckActions();
	bool canMergeCurrent() const;
	bool canEditLayer(const QModelIndex &idx) const;

	void
	currentChanged(const QModelIndex &current, const QModelIndex &previous);
	void selectionChanged(
		const QItemSelection &selected, const QItemSelection &deselected);
	void updateCurrent(const QModelIndex &current);
	void updateUiFromCurrent();

	void addOrPromptLayerOrGroup(bool group);
	void addLayerOrGroupFromPrompt(
		int selectedId, bool group, const QString &title, int opacityPercent,
		int blendMode, bool isolated, bool censored, bool defaultLayer,
		bool visible, int sketchOpacityPercent, const QColor &sketchTint);
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

	void showPropertiesForNew(bool group);
	void showPropertiesOfCurrent();
	void showPropertiesOfIndex(const QModelIndex &index);
	dialogs::LayerProperties *makeLayerPropertiesDialog(
		const QString &dialogObjectName, const QModelIndex &index);

	void showSketchTintColorPicker();
	void setUpdateSketchTint(const QColor &tint);

	bool isGroupSelected() const;
	QModelIndex currentSelection() const;
	bool ownsAllTopLevelSelections() const;
	QModelIndexList topLevelSelections() const;
	QSet<int> topLevelSelectedIds() const;
	void
	gatherTopLevel(const std::function<void(const QModelIndex &)> &fn) const;
	void selectLayerIndex(QModelIndex index, bool scrollTo = false);

	QString layerCreatorName(uint16_t layerId) const;

	QString getBaseName(bool group);

	canvas::CanvasModel *m_canvas;

	// cache selection and remember it across model resets
	int m_currentId;
	int m_nearestToDeletedId;
	QSet<int> m_selectedIds;

	// try to retain view status across model resets
	QSet<int> m_expandedGroups;
	int m_lastScrollPosition;

	int m_trackId;
	int m_frame;

	bool m_noupdate;
	bool m_sketchMode = false;

	QMenu *m_contextMenu;
	LayerAclMenu *m_aclmenu;

	QTimer *m_debounceTimer;
	int m_updateBlendModeIndex;
	int m_updateOpacity;
	int m_updateSketchOpacity = -1;
	QColor m_updateSketchTint;

	widgets::GroupedToolButton *m_lockButton;
	QComboBox *m_blendModeCombo;
	KisSliderSpinBox *m_opacitySlider;
	widgets::GroupedToolButton *m_sketchButton;
	widgets::GroupedToolButton *m_sketchTintButton;
	QTreeView *m_view;

	Actions m_actions;
};

}

#endif
