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
#ifndef LAYERLISTDOCK_H
#define LAYERLISTDOCK_H

#include "libclient/canvas/features.h"

#include <QDockWidget>

class QModelIndex;
class QItemSelection;
class QMenu;
class QTimer;
class QTreeView;

class Ui_LayerBox;

namespace net {
	class Envelope;
}

namespace canvas {
	class CanvasModel;
	enum class Feature;
}

namespace widgets {
	class GroupedToolButton;
}
namespace docks {

class LayerAclMenu;

class LayerList : public QDockWidget
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

signals:
	//! A layer was selected by the user
	void layerSelected(int id);
	void activeLayerVisibilityChanged();

	void layerCommand(const net::Envelope &msg);

private slots:
	void beforeLayerReset();
	void afterLayerReset();
	
	void onFeatureAccessChange(canvas::Feature feature, bool canuse);

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
	void changeLayerAcl(bool lock, canvas::Tier tier, QVector<uint8_t> exclusive);

	void lockStatusChanged(int layerId);
	void selectionChanged(const QItemSelection &selected);

private:
	void updateLockedControls();
	bool canMergeCurrent() const;

	void updateUiFromSelection();

	QModelIndex currentSelection() const;
	void selectLayerIndex(QModelIndex index, bool scrollTo=false);

	QString layerCreatorName(uint16_t layerId) const;

	canvas::CanvasModel *m_canvas;

	// cache selection and remember it across model resets
	int m_selectedId;
	int m_nearestToDeletedId;

	// try to retain view status across model resets
	QVector<int> m_expandedGroups;
	int m_lastScrollPosition;

	bool m_noupdate;

	QMenu *m_contextMenu;
	LayerAclMenu *m_aclmenu;

	widgets::GroupedToolButton *m_lockButton;
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

