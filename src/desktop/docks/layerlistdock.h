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

#include "canvas/features.h"
#include "dialogs/layerproperties.h"

#include <QDockWidget>

class QModelIndex;
class QItemSelection;
class QMenu;
class QTimer;

class Ui_LayerBox;

namespace net {
	class Envelope;
}

namespace canvas {
	struct LayerListItem;
	class CanvasModel;
	enum class Feature;
}

namespace docks {

class LayerAclMenu;

class LayerList : public QDockWidget
{
Q_OBJECT
public:
	LayerList(QWidget *parent=nullptr);
	~LayerList();

	void setCanvas(canvas::CanvasModel *canvas);

	//! These actions are shown in a menu outside this dock
	void setLayerEditActions(QAction *add, QAction *duplicate, QAction *merge, QAction *del);

	/**
	 * Is the currently selected layer locked for editing?
	 *
	 * This may be because it is actually locked or because it is hidden or a
	 * non-editable (group) layer.
	 */
	bool isCurrentLayerLocked() const;

public slots:
	void selectLayer(int id);
	void showLayerNumbers(bool show);

	void selectAbove();
	void selectBelow();

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
	void duplicateLayer();
	void deleteSelected();
	void setSelectedDefault();
	void mergeSelected();
	void showPropertiesOfSelected();
	void showPropertiesOfIndex(QModelIndex index);
	void opacityAdjusted();
	void blendModeChanged();
	void hideSelected();
	void censorSelected(bool censor);
	void setSelectedFixed(bool fixed);
	void setLayerVisibility(int layerId, bool visible);
	void changeLayerAcl(bool lock, canvas::Tier tier, QVector<uint8_t> exclusive);

	void lockStatusChanged(int layerId);
	void selectionChanged(const QItemSelection &selected);
	void layerContextMenu(const QPoint &pos);

	void sendOpacityUpdate();

	void emitPropertyChangeCommands(const dialogs::LayerProperties::ChangedLayerData &c);

private:
	void updateLockedControls();
	bool canMergeCurrent() const;

	void updateUiFromSelection();

	QModelIndex currentSelection() const;
	void selectLayerIndex(QModelIndex index, bool scrollTo=false);

	Ui_LayerBox *m_ui;
	canvas::CanvasModel *m_canvas;

	int m_selectedId;
	//int m_lastSelectedRow;

	// try to retain view status across model resets
	QVector<int> m_expandedGroups;
	int m_lastScrollPosition;

	bool m_noupdate;

	dialogs::LayerProperties *m_layerProperties;
	LayerAclMenu *m_aclmenu;
	QMenu *m_layermenu;

	QAction *m_addLayerAction;
	QAction *m_duplicateLayerAction;
	QAction *m_mergeLayerAction;
	QAction *m_deleteLayerAction;

	QAction *m_menuSeparator;
	QAction *m_menuHideAction;
	QAction *m_menuPropertiesAction;
	QAction *m_menuDefaultAction;
	QAction *m_menuFixedAction;

	QTimer *m_opacityUpdateTimer;
};

}

#endif

