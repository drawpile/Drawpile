/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2017 Calle Laakkonen

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
#ifndef DP_STATETRACKER_H
#define DP_STATETRACKER_H

#include "retcon.h"
#include "history.h"
#include "core/point.h"

#include <QObject>

namespace protocol {
	class CanvasResize;
	class CanvasBackground;
	class LayerCreate;
	class LayerAttributes;
	class LayerVisibility;
	class LayerRetitle;
	class LayerOrder;
	class LayerDelete;
	class DefaultLayer;
	class ToolChange;
	class PenMove;
	class PenUp;
	class DrawDabsClassic;
	class DrawDabsPixel;
	class PutImage;
	class PutTile;
	class FillRect;
	class UndoPoint;
	class Undo;
	class AnnotationCreate;
	class AnnotationReshape;
	class AnnotationEdit;
	class AnnotationDelete;
	class MoveRegion;
}

namespace paintcore {
	class LayerStack;
	class Savepoint;
}

class QTimer;

namespace canvas {

class StateTracker;
class CanvasModel;

/**
 * @brief A snapshot of the statetracker state.
 *
 * This is used for undo/redo as well as jumping around in indexed recordings.
 */
class StateSavepoint {
	friend class StateTracker;
public:
	StateSavepoint() : m_data(nullptr) {}
	StateSavepoint(const StateSavepoint &sp);
	StateSavepoint &operator=(const StateSavepoint &sp);
	~StateSavepoint();

	void toDatastream(QDataStream &ds) const;
	static StateSavepoint fromDatastream(QDataStream &ds, StateTracker *owner);

	bool operator!() const { return !m_data; }
	bool operator==(const StateSavepoint &sp) const { return m_data == sp.m_data; }
	bool operator!=(const StateSavepoint &sp) const { return m_data != sp.m_data; }

	//! Get this snapshots timestamp
	qint64 timestamp() const;

	/**
	 * @brief Get a thumbnail of savepoint content
	 * @param maxSize maximum thumbnail size
	 */
	QImage thumbnail(const QSize &maxSize) const;

	/**
	 * @brief Generate a list of commands to initialize a session to this savepoint
	 *
	 * (Used when resetting a session to a prior state.)
	 *
	 * @param contextId context Id of the user resetting
	 * @param canvas if set, ACL settings are got from here
	 */
	QList<protocol::MessagePtr> initCommands(uint8_t contextId, CanvasModel *canvas) const;

private:
	struct Data;

	const Data *operator ->() const { return m_data; }
	Data *operator ->();

	Data *m_data;
};

}

Q_DECLARE_TYPEINFO(canvas::StateSavepoint, Q_MOVABLE_TYPE);

namespace canvas {

class LayerListModel;

/**
 * \brief Drawing context state tracker
 * 
 * The state tracker object keeps track of each drawing context and performs
 * the drawing using the paint engine.
 */
class StateTracker : public QObject {
	Q_OBJECT
public:
	StateTracker(paintcore::LayerStack *image, LayerListModel *layerlist, int myId, QObject *parent=0);
	StateTracker(const StateTracker &) = delete;
	~StateTracker();

	void localCommand(protocol::MessagePtr msg);
	void receiveCommand(protocol::MessagePtr msg);
	void receiveQueuedCommand(protocol::MessagePtr msg);

	void endRemoteContexts();
	void endPlayback();

	//! Reset the entire history
	void reset();

	bool hasFullHistory() const { return m_fullhistory; }
	const History &getHistory() const { return m_history; }

	/**
	 * @brief Set if all user markers (own included) should be shown
	 * @param showall
	 */
	void setShowAllUserMarkers(bool showall) { _showallmarkers = showall; }

	/**
	 * @brief Get the local user's ID
	 * @return
	 */
	int localId() const { return m_myId; }

	/**
	 * @brief Set the local user's ID
	 */
	void setLocalId(int id) { m_myId = id; }

	/**
	 * @brief Get the paint canvas
	 * @return
	 */
	paintcore::LayerStack *image() const { return _image; }

	/**
	 * @brief Check if the given layer is locked.
	 *
	 * Note. This information should only be used for the UI and not
	 * for filtering events! Any command sent by the server should be
	 * executed, even if we think the target layer is locked!
	 *
	 * @param id layer ID
	 * @return true if the layer is locked
	 */
	bool isLayerLocked(int id) const;

	//! Has the local user participated in the session yet?
	bool hasParticipated() const { return m_hasParticipated; }

	StateTracker &operator=(const StateTracker&) = delete;

	/**
	 * @brief Create a new savepoint
	 *
	 * This is used internally for undo/redo as well as when
	 * saving snapshots for an indexed recording.
	 * @return
	 */
	StateSavepoint createSavepoint(int pos);

	/**
	 * @brief Reset state to the given save point
	 *
	 * This is used when jumping inside a recording. Calling this
	 * will reset session history
	 * @param sp
	 */
	void resetToSavepoint(StateSavepoint sp);

	//! Get all existing savepoints (can be used for selecting a reset point)
	QList<StateSavepoint> getSavepoints() const { return m_savepoints; }

signals:
	void myAnnotationCreated(int id);
	void layerAutoselectRequest(int);

	void userMarkerMove(int id, int layerId, const QPoint &point);
	void userMarkerHide(int id);

	void catchupProgress(int percent);
	void sequencePoint(int);

public slots:
	void previewLayerOpacity(int id, float opacity);

	/**
	 * @brief Set the "local user is currently drawing!" hint
	 *
	 * This affects the way the retcon local fork is handled: when
	 * local drawing is in progress, the local fork is not discarded
	 * on conflict (with other users) to avoid self-conflict feedback loop.
	 *
	 * Not setting this flag doesn't break anything, but may cause
	 * unnecessary rollbacks if a conflict occurs during local drawing.
	 */
	void setLocalDrawingInProgress(bool pendown) { m_localPenDown = pendown; }

private slots:
	void processQueuedCommands();

private:
	void handleCommand(protocol::MessagePtr msg, bool replay, int pos);

	AffectedArea affectedArea(const protocol::MessagePtr msg) const;

	// Layer related commands
	void handleCanvasResize(const protocol::CanvasResize &cmd, int pos);
	void handleCanvasBackground(const protocol::CanvasBackground &cmd);
	void handleLayerCreate(const protocol::LayerCreate &cmd);
	void handleLayerAttributes(const protocol::LayerAttributes &cmd);
	void handleLayerVisibility(const protocol::LayerVisibility &cmd);
	void handleLayerTitle(const protocol::LayerRetitle &cmd);
	void handleLayerOrder(const protocol::LayerOrder &cmd);
	void handleLayerDelete(const protocol::LayerDelete &cmd);
	void handleLayerDefault(const protocol::DefaultLayer &cmd);
	
	// Drawing related commands
	void handleDrawDabs(const protocol::Message &msg);
	void handlePenUp(const protocol::PenUp &cmd);
	void handlePutImage(const protocol::PutImage &cmd);
	void handlePutTile(const protocol::PutTile &cmd);
	void handleFillRect(const protocol::FillRect &cmd);
	void handleMoveRegion(const protocol::MoveRegion &cmd);

	// Undo/redo
	void handleUndoPoint(const protocol::UndoPoint &cmd, bool replay, int pos);
	void handleUndo(protocol::Undo &cmd);
	void makeSavepoint(int pos);
	void revertSavepointAndReplay(const StateSavepoint savepoint);

	// Annotation related commands
	void handleAnnotationCreate(const protocol::AnnotationCreate &cmd);
	void handleAnnotationReshape(const protocol::AnnotationReshape &cmd);
	void handleAnnotationEdit(const protocol::AnnotationEdit &cmd);
	void handleAnnotationDelete(const protocol::AnnotationDelete &cmd);

	paintcore::LayerStack *_image;
	LayerListModel *m_layerlist;

	QString _title;
	int m_myId;
	int m_myLastLayer;

	History m_history;
	QList<StateSavepoint> m_savepoints;

	LocalFork m_localfork;

	bool m_fullhistory;
	bool _showallmarkers;
	bool m_hasParticipated;
	bool m_localPenDown;

	QList<protocol::MessagePtr> m_msgqueue;
	QTimer *m_queuetimer;
	bool m_isQueued;
};

}

#endif
