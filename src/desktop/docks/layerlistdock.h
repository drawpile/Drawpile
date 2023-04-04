// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LAYERLISTDOCK_H
#define LAYERLISTDOCK_H

extern "C" {
#include <dpmsg/acl.h>
}

#include <QDockWidget>

class QModelIndex;
class QItemSelection;
class QMenu;
class QTimer;
class QTreeView;
class QComboBox;
class QSlider;
class KisSliderSpinBox;

namespace canvas {
	class CanvasModel;
}

namespace drawdance {
	class Message;
}

namespace widgets {
	class GroupedToolButton;
}

namespace docks {

class LayerAclMenu;

class LayerList final : public QDockWidget
{
Q_OBJECT
public:
	LayerList(QWidget *parent=nullptr);

	void setCanvas(canvas::CanvasModel *canvas);

	//! These actions are shown in a menu outside this dock
	void setLayerEditActions(QAction *addLayer, QAction *addGroup, QAction *duplicate, QAction *merge, QAction *properties, QAction *del);

	/**
	 * Is the currently selected layer locked for editing?
	 *
	 * This may be because it is actually locked or because it is hidden or a
	 * non-editable (group) layer.
	 */
	bool isCurrentLayerLocked() const;

public slots:
	void selectLayer(int id);
	void selectAbove();
	void selectBelow();

signals:
	//! A layer was selected by the user
	void layerSelected(int id);
	void activeLayerVisibilityChanged();

	void layerCommands(int count, const drawdance::Message *msgs);

private slots:
	void beforeLayerReset();
	void afterLayerReset();

	void onFeatureAccessChange(DP_Feature feature, bool canuse);

	void addLayer();
	void addGroup();
	void duplicateLayer();
	void deleteSelected();
	void mergeSelected();

	void showPropertiesOfSelected();
	void showPropertiesOfIndex(QModelIndex index);
	void showContextMenu(const QPoint &pos);
	void censorSelected(bool censor);
	void setLayerVisibility(int layerId, bool visible);
	void changeLayerAcl(bool lock, DP_AccessTier tier, QVector<uint8_t> exclusive);

	void layerLockStatusChanged(int layerId);
	void userLockStatusChanged(bool);
	void blendModeChanged(int index);
	void opacityChanged(int value);
	void selectionChanged(const QItemSelection &selected);

	void triggerUpdate();

private:
	void updateLockedControls();
	void updateBlendModes(bool compatibilityMode);
	bool canMergeCurrent() const;

	void updateUiFromSelection();

	void addLayerOrGroup(bool group);

	QModelIndex currentSelection() const;
	void selectLayerIndex(QModelIndex index, bool scrollTo=false);

	QString layerCreatorName(uint16_t layerId) const;

	canvas::CanvasModel *m_canvas;

	// cache selection and remember it across model resets
	int m_selectedId;
	int m_nearestToDeletedId;

	// try to retain view status across model resets
	QHash<int, QPair<QString, qint64>> m_expandedGroups;
	int m_lastScrollPosition;

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

	QAction *m_addLayerAction;
	QAction *m_addGroupAction;
	QAction *m_duplicateLayerAction;
	QAction *m_mergeLayerAction;
	QAction *m_propertiesAction;
	QAction *m_deleteLayerAction;
};

}

#endif

