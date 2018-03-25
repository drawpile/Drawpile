/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2018 Calle Laakkonen

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

#include <QDockWidget>

class QModelIndex;
class QItemSelection;
class QMenu;
class QTimer;

class Ui_LayerBox;

namespace protocol {
	class MessagePtr;
}

namespace canvas {
	struct LayerListItem;
	class CanvasModel;
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

	//! Get the ID of the currently selected layer
	int currentLayer();

	bool isCurrentLayerLocked() const;

	void setOperatorMode(bool op);
	void setControlsLocked(bool locked);
	void setOwnLayers(bool own);

public slots:
	void selectLayer(int id);
	void showLayerNumbers(bool show);

	void selectAbove();
	void selectBelow();

signals:
	//! A layer was selected by the user
	void layerSelected(int id);
	void activeLayerVisibilityChanged();

	void layerCommand(protocol::MessagePtr msg);

private slots:
	void onLayerCreate(const QModelIndex &parent, int first, int last);
	void beforeLayerDelete();
	void onLayerDelete(const QModelIndex &parent, int first, int last);
	void onLayerReorder();

	void addLayer();
	void insertLayer();
	void duplicateLayer();
	void deleteSelected();
	void setSelectedDefault();
	void mergeSelected();
	void renameSelected();
	void opacityAdjusted();
	void blendModeChanged();
	void hideSelected();
	void setLayerVisibility(int layerId, bool visible);
	void changeLayerAcl(bool lock, QList<uint8_t> exclusive);

	void dataChanged(const QModelIndex &topLeft, const QModelIndex & bottomRight);
	void selectionChanged(const QItemSelection &selected);
	void layerContextMenu(const QPoint &pos);

	void sendOpacityUpdate();

private:
	void updateLockedControls();
	bool canMergeCurrent() const;

	QModelIndex currentSelection() const;

	Ui_LayerBox *m_ui;
	canvas::CanvasModel *m_canvas;
	int m_selectedId;
	int m_lastSelectedRow;
	bool m_noupdate;
	LayerAclMenu *m_aclmenu;
	QMenu *m_layermenu;

	QAction *m_addLayerAction;
	QAction *m_duplicateLayerAction;
	QAction *m_mergeLayerAction;
	QAction *m_deleteLayerAction;

	QAction *m_menuInsertAction;
	QAction *m_menuHideAction;
	QAction *m_menuRenameAction;
	QAction *m_menuDefaultAction;

	bool m_op;
	bool m_lockctrl;
	bool m_ownlayers;
	QTimer *m_opacityUpdateTimer;
};

}

#endif

