/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2015 Calle Laakkonen

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

namespace net {
	class Client;
	struct LayerListItem;
}

namespace docks {

class LayerAclMenu;

class LayerList : public QDockWidget
{
Q_OBJECT
public:
	LayerList(QWidget *parent=0);

	void setClient(net::Client *client);

	//! Initialize the widget for a new session
	void init();

	//! Get the ID of the currently selected layer
	int currentLayer();

	bool isCurrentLayerLocked() const;

	void setOperatorMode(bool op);
	void setControlsLocked(bool locked);

public slots:
	void selectLayer(int id);

	void selectAbove();
	void selectBelow();

signals:
	//! A layer was selected by the user
	void layerSelected(int id);

	void layerViewModeSelected(int mode);

private slots:
	void onLayerCreate(bool wasfirst);
	void onLayerDelete(int id, int idx);
	void onLayerReorder();

	void addLayer();
	void insertLayer();
	void duplicateLayer();
	void deleteOrMergeSelected();
	void deleteSelected();
	void mergeSelected();
	void renameSelected();
	void opacityAdjusted();
	void blendModeChanged();
	void hideSelected();
	void setLayerVisibility(int layerId, bool visible);
	void changeLayerAcl(bool lock, QList<uint8_t> exclusive);
	void layerViewModeTriggered(QAction *act);
	void showLayerNumbers(bool show);

	void dataChanged(const QModelIndex &topLeft, const QModelIndex & bottomRight);
	void selectionChanged(const QItemSelection &selected);
	void layerContextMenu(const QPoint &pos);

	void sendOpacityUpdate();

private:
	void updateLockedControls();
	bool canMergeCurrent() const;

	QModelIndex currentSelection() const;

	net::Client *_client;
	int _selected;
	Ui_LayerBox *_ui;
	bool _noupdate;
	LayerAclMenu *_aclmenu;
	QMenu *_layermenu;

	QAction *_addLayerAction;
	QAction *_duplicateLayerAction;
	QAction *_deleteLayerAction;
	QAction *_showNumbersAction;

	QMenu *_viewMode;

	QAction *_menuInsertAction;
	QAction *_menuHideAction;
	QAction *_menuDeleteAction;
	QAction *_menuMergeAction;
	QAction *_menuRenameAction;

	bool _op;
	bool _lockctrl;
	QTimer *_opacityUpdateTimer;
};

}

#endif

