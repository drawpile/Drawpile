/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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
#ifndef CANVAS_SCENE_H
#define CANVAS_SCENE_H

#include <QGraphicsScene>

#include "../shared/net/message.h"

namespace paintcore {
	class LayerStack;
	class Brush;
}

namespace net {
	class Client;
}

class QGraphicsItem;

//! Drawing board related classes
namespace drawingboard {

class StateTracker;
class CanvasItem;
class AnnotationState;
class AnnotationItem;
class SelectionItem;
class UserMarkerItem;
class LaserTrailItem;

/**
 * @brief The drawing board
 * The drawing board contains the picture and provides methods
 * to modify it.
 */
class CanvasScene : public QGraphicsScene
{
	Q_OBJECT

public:
	CanvasScene(QObject *parent);
	~CanvasScene();

	//! Clear and initialize the canvas
	void initCanvas(net::Client *client);

	//! Get canvas width
	int width() const;

	//! Get canvas height
	int height() const;

	//! Get the layers
	paintcore::LayerStack *layers();

	//! Get canvas contents as an image
	QImage image() const;

	/**
	 * @brief Get the current selection as an image
	 *
	 * If there is no selection, the whole canvas is copied
	 * If a valid layer ID is provided, only the contents of that layer is copied.
	 *
	 * @param layer layer ID
	 */
	QImage selectionToImage(int layer);

	//! Create a new selection and paste an image from the clipboard
	void pasteFromImage(const QImage &image, const QPoint &defaultPoint);

	//! Save the canvas to a file
	bool save(const QString& filename) const;

	//! Check if we are using features that require saving in OpenRaster format to preserve
	bool needSaveOra() const;

	//! Is there an image on the drawing board
	bool hasImage() const;

	//! Return the annotation at the given coordinates (if any)
	AnnotationItem *annotationAt(const QPoint &point);

	/**
	 * @brief Find an unused annotation ID
	 * @return available ID or 0 if all are taken
	 */
	int getAvailableAnnotationId() const;

	//! Get a list of annotations with no content
	QList<int> listEmptyAnnotations() const;

	//! Are annotation borders shown?
	bool showAnnotationBorders() const { return _showAnnotationBorders; }

	//! Show/hide annotations
	void showAnnotations(bool show);

	//! Set the graphics item used for tool preview
	void setToolPreview(QGraphicsItem *item);

	//! Get the current tool preview item
	QGraphicsItem *toolPreview() { return _toolpreview; }

	//! Set the selection
	void setSelectionItem(SelectionItem *selection);
	void setSelectionItem(const QRect &rect);

	SelectionItem *selectionItem() { return _selection; }

	/**
	 * @brief Pick a color at the given coordinates.
	 *
	 * Emits colorPicked
	 * @param x X coordinate
	 * @param y Y coordinate
	 * @param layer layer ID. If 0, the merged pixel value is picked.
	 * @param bg pick background color
	 */
	void pickColor(int x, int y, int layer, bool bg);

	/**
	 * @brief Get the state tracker for this session.
	 *
	 * Note! The state tracker is deleted when this board is reinitialized!
	 * @return state tracker instance
	 */
	StateTracker *statetracker() const { return _statetracker; }

	/**
	 * @brief Get a QPen that resembles the given brush
	 *
	 * This is used for preview strokes
	 * @param brush
	 * @return qpen
	 */
	static QPen penForBrush(const paintcore::Brush &brush);

	/**
	 * @brief Set the session title
	 * @param title
	 */
	void setTitle(const QString &title);

	/**
	 * @brief Get the session title
	 * @return
	 */
	QString title() const;

public slots:
	//! Show annotation borders
	void showAnnotationBorders(bool hl);

	//! Show/hide remote cursor markers
	void showUserMarkers(bool show);

	//! Show layer selection in cursor marker
	void showUserLayers(bool show);

	//! Show hide laser pointer trails
	void showLaserTrails(bool show);

	void handleLocalCommand(protocol::MessagePtr cmd);
	void handleDrawingCommand(protocol::MessagePtr cmd);

	//! Generate a snapshot point and send it to the server
	void sendSnapshot(bool forcenew);

	void moveUserMarker(int id, const QPointF &point, int trail);
	void setUserMarkerName(int id, const QString &name);
	void setUserMarkerAttribs(int id, const QColor &color, const QString &layer);
	void hideUserMarker(int id=-1);

signals:
	void canvasInitialized();

	//! Canvas size has just changed
	void canvasResized(int xoffset, int yoffset, const QSize &oldSize);

	//! User used a color picker tool on this scene
	void colorPicked(const QColor &color, bool background);

	//! An annotation was just created (by the local user)
	void myAnnotationCreated(AnnotationItem *item);

	//! Request to select a layer (in response to layer creation)
	void layerAutoselectRequest(int id);

	//! An annotation was just deleted
	void annotationDeleted(int id);

	//! Emitted when a canvas modifying command is received
	void canvasModified();

	//! Emitted when a new snapshot point was generated
	void newSnapshot(QList<protocol::MessagePtr>);

private slots:
	void handleCanvasResize(int xoffset, int yoffset, const QSize &oldsize);
	void handleAnnotationChange(int id);
	void advanceUsermarkerAnimation();

private:
	UserMarkerItem *getOrCreateUserMarker(int id);
	AnnotationItem *getAnnotationItem(int id);

	//! The board contents
	CanvasItem *_image;

	//! Drawing context state tracker
	StateTracker *_statetracker;

	//! Laser pointer trails
	QList<LaserTrailItem*> _lasertrails;

	//! Graphics item for previewing a special tool shape
	QGraphicsItem *_toolpreview;

	//! Current selection
	SelectionItem *_selection;

	//! User markers display remote user cursor positions
	QHash<int, UserMarkerItem*> _usermarkers;

	QTimer *_animTickTimer;

	bool _showAnnotations;
	bool _showAnnotationBorders;
	bool _showUserMarkers;
	bool _showUserLayers;
	bool _showLaserTrails;
};

}

#endif

