/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/
#ifndef DP_STATETRACKER_H
#define DP_STATETRACKER_H

#include <QObject>
#include <QHash>

#include "core/brush.h"
#include "core/point.h"
#include "../shared/net/message.h"
#include "../shared/net/messagestream.h"

namespace protocol {
	class CanvasResize;
	class LayerCreate;
	class LayerAttributes;
	class LayerRetitle;
	class LayerOrder;
	class LayerDelete;
	class ToolChange;
	class PenMove;
	class PenUp;
	class PutImage;
	class UndoPoint;
	class Undo;
	class AnnotationCreate;
	class AnnotationReshape;
	class AnnotationEdit;
	class AnnotationDelete;
}

namespace paintcore {
	class LayerStack;
	class Savepoint;
}

namespace net {
	class Client;
	class LayerListModel;
}

namespace drawingboard {

class CanvasScene;
class AnnotationItem;

struct ToolContext {
	int layer_id;
	paintcore::Brush brush;
};

/**
 * \brief User state
 * 
 * The drawing context captures the state needed by a single user for drawing.
 */
struct DrawingContext {
	DrawingContext() : pendown(false), distance_accumulator(0) {}
	
	//! Currently selected tool
	ToolContext tool;
	
	//! Last pen-move point
	paintcore::Point lastpoint;
	
	//! Is the stroke currently in progress?
	bool pendown;
	
	//! Stroke length (used for dab spacing)
	qreal distance_accumulator;
};

class StateSavepoint;

/**
 * \brief Drawing context state tracker
 * 
 * The state tracker object keeps track of each drawing context and performs
 * the drawing using the paint engine.
 */
class StateTracker : public QObject {
	Q_OBJECT
public:
	StateTracker(CanvasScene *scene, net::Client *client, QObject *parent=0);
	StateTracker(const StateTracker &) = delete;
	~StateTracker();

	void receiveCommand(protocol::MessagePtr msg);

	void endRemoteContexts();

	QList<protocol::MessagePtr> generateSnapshot(bool forcenew);

	const QHash<int, DrawingContext> &drawingContexts() const { return _contexts; }

	/**
	 * @brief Set the maximum length of the stored history.
	 * @param length
	 */
	void setMaxHistorySize(uint limit) { _msgstream_sizelimit = limit; }

	/**
	 * @brief Set if all user markers (own included) should be shown
	 * @param showall
	 */
	void setShowAllUserMarkers(bool showall) { _showallmarkers = showall; }

	StateTracker &operator=(const StateTracker&) = delete;

signals:
	void myAnnotationCreated(AnnotationItem *item);
	void myLayerCreated(int);

private:
	void handleCommand(protocol::MessagePtr msg, bool replay, int pos);

	// Layer related commands
	void handleCanvasResize(const protocol::CanvasResize &cmd, int pos);
	void handleLayerCreate(const protocol::LayerCreate &cmd);
	void handleLayerAttributes(const protocol::LayerAttributes &cmd);
	void handleLayerTitle(const protocol::LayerRetitle &cmd);
	void handleLayerOrder(const protocol::LayerOrder &cmd);
	void handleLayerDelete(const protocol::LayerDelete &cmd);
	
	// Drawing related commands
	void handleToolChange(const protocol::ToolChange &cmd);
	void handlePenMove(const protocol::PenMove &cmd);
	void handlePenUp(const protocol::PenUp &cmd);
	void handlePutImage(const protocol::PutImage &cmd);

	// Undo/redo
	void handleUndoPoint(const protocol::UndoPoint &cmd, bool replay, int pos);
	void handleUndo(protocol::Undo &cmd);
	bool canMakeSavepoint(int pos) const;
	void makeSavepoint(int pos);
	void revertSavepoint(const StateSavepoint *savepoint);

	// Annotation related commands
	void handleAnnotationCreate(const protocol::AnnotationCreate &cmd);
	void handleAnnotationReshape(const protocol::AnnotationReshape &cmd);
	void handleAnnotationEdit(const protocol::AnnotationEdit &cmd);
	void handleAnnotationDelete(const protocol::AnnotationDelete &cmd);

	QHash<int, DrawingContext> _contexts;
	
	CanvasScene *_scene;
	paintcore::LayerStack *_image;
	net::LayerListModel *_layerlist;

	int _myid;

	protocol::MessageStream _msgstream;
	QList<StateSavepoint*> _savepoints;
	uint _msgstream_sizelimit;
	bool _hassnapshot;
	bool _showallmarkers;
};

}

#endif
