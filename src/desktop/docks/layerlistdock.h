// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LAYERLISTDOCK_H
#define LAYERLISTDOCK_H
extern "C" {
#include <dpmsg/acl.h>
}
#include "desktop/docks/dockbase.h"
#include "desktop/scene/canvasview.h"
#include <QSet>

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
}

namespace net {
class Message;
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
		QAction *setFillSource = nullptr;
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
	QFlags<widgets::CanvasView::Lock> currentLayerLock() const;

public slots:
	void selectLayer(int id);
	void selectAbove();
	void selectBelow();

	void setTrackId(int trackId);
	void setFrame(int frame);

signals:
	//! A layer was selected by the user
	void layerSelected(int id);
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
	void setFillSourceToSelected();

	void showPropertiesOfSelected();
	void showPropertiesOfIndex(QModelIndex index);
	void showContextMenu(const QPoint &pos);
	void censorSelected(bool censor);
	void disableAutoselectAny();
	void setLayerVisibility(int layerId, bool visible);
	void
	changeLayerAcl(bool lock, DP_AccessTier tier, QVector<uint8_t> exclusive);

	void layerLockStatusChanged(int layerId);
	void userLockStatusChanged(bool);
	void blendModeChanged(int index);
	void opacityChanged(int value);
	void selectionChanged(const QItemSelection &selected);

	void triggerUpdate();

private:
	void updateActionLabels();
	void setActionLabel(QAction *action, const QString &text);
	void updateLockedControls();
	void updateBlendModes(bool compatibilityMode);
	bool canMergeCurrent() const;

	void updateUiFromSelection();

	void addLayerOrGroup(
		bool group, bool duplicateKeyFrame, bool keyFrame, int keyFrameOffset);
	int intuitKeyFrameTarget(
		int sourceFrame, int targetFrame, int &sourceId, int &targetId,
		uint8_t &flags);

	bool isGroupSelected() const;
	QModelIndex currentSelection() const;
	void selectLayerIndex(QModelIndex index, bool scrollTo = false);

	QString layerCreatorName(uint16_t layerId) const;

	canvas::CanvasModel *m_canvas;

	// cache selection and remember it across model resets
	int m_selectedId;
	int m_nearestToDeletedId;

	// try to retain view status across model resets
	QSet<int> m_expandedGroups;
	int m_lastScrollPosition;

	int m_trackId;
	int m_frame;

	bool m_noupdate;

	QMenu *m_contextMenu;
	LayerAclMenu *m_aclmenu;

	QTimer *m_debounceTimer;
	int m_updateBlendModeIndex;
	int m_updateOpacity;

	widgets::GroupedToolButton *m_lockButton;
	QComboBox *m_blendModeCombo;
	KisSliderSpinBox *m_opacitySlider;
	QTreeView *m_view;

	Actions m_actions;
};

}

#endif
