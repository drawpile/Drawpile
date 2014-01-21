/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2009 Calle Laakkonen

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
#ifndef CANVAS_SCENE_H
#define CANVAS_SCENE_H

#include <QGraphicsScene>

#include "core/point.h"
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
class Preview;

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

	//! Check if there are any annotations
	bool hasAnnotations() const;

	//! Return the annotation at the given coordinates (if any)
	AnnotationItem *annotationAt(const QPoint &point);

	//! Return the annotation with the given ID
	AnnotationItem *getAnnotationById(int id);

	//! Get all annotations
	QList<AnnotationItem*> getAnnotations(bool onlyVisible=false) const;

	//! Reset annotations
	void setAnnotations(const QVector<AnnotationState> &annotations);

	//! Are annotation borders shown?
	bool showAnnotationBorders() const { return _showAnnotationBorders; }

	//! Show/hide annotations
	void showAnnotations(bool show);

	//! Delete an annotation with the specific ID. Triggers annotationDeleted signal
	bool deleteAnnotation(int id);

	//! Remove all annotations
	void clearAnnotations();

	//! Set the graphics item used for tool preview
	void setToolPreview(QGraphicsItem *item);

	//! Get the current tool preview item
	QGraphicsItem *toolPreview() { return _toolpreview; }

	//! Set the selection
	void setSelectionItem(SelectionItem *selection);

	SelectionItem *selectionItem() { return _selection; }

	//! Start a new preview stroke
	void startPreview(const paintcore::Brush &brush, const paintcore::Point &point);

	//! Add a preview stroke
	void addPreview(const paintcore::Point& point);

	//! Remove the oldest preview stroke(s)
	void takePreview(int count);

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
	StateTracker *statetracker() { return _statetracker; }

	void hideUserMarker(int id=-1);

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
	void setTitle(const QString &title) { _title = title; }

	/**
	 * @brief Get the session title
	 * @return
	 */
	const QString title() const { return _title; }

public slots:
	//! Show annotation borders
	void showAnnotationBorders(bool hl);

	//! Show/hide remote cursor markers
	void showUserMarkers(bool show);

	void handleDrawingCommand(protocol::MessagePtr cmd);

	//! Generate a snapshot point and send it to the server
	void sendSnapshot(bool forcenew);

	//! Clear out all preview strokes
	void clearPreviews();

	void moveUserMarker(int id, int x, int y, int trail);
	void setUserMarkerName(int id, const QString &name);
	void setUserMarkerColor(int id, const QColor &color);

signals:
	void canvasInitialized();

	//! User used a color picker tool on this scene
	void colorPicked(const QColor &color, bool background);

	//! An annotation was just created (by the local user)
	void myAnnotationCreated(AnnotationItem *item);

	//! A new layer was just created (by the local user)
	void myLayerCreated(int id);

	//! An annotation was just deleted
	void annotationDeleted(int id);

	//! Emitted when a canvas modifying command is received
	void canvasModified();

	//! Emitted when a new snapshot point was generated
	void newSnapshot(QList<protocol::MessagePtr>);

private slots:
	void handleCanvasResize(int xoffset, int yoffset);
	void advanceUsermarkerAnimation();

private:
	UserMarkerItem *getOrCreateUserMarker(int id);

	//! The board contents
	CanvasItem *_image;

	//! Drawing context state tracker
	StateTracker *_statetracker;

	//! Preview strokes currently on screen
	QList<QGraphicsLineItem*> _previewstrokes;

	//! Cache of reusable preview strokes
	QList<QGraphicsLineItem*> _previewstrokecache;

	//! Laser pointer trails
	QList<LaserTrailItem*> _lasertrails;

	//! Coordinate of the last preview stroke
	paintcore::Point _lastpreview;

	//! The pen to use for preview strokes
	QPen _previewpen;

	//! Graphics item for previewing a special tool shape
	QGraphicsItem *_toolpreview;

	//! Current selection
	SelectionItem *_selection;

	//! User markers display remote user cursor positions
	QHash<int, UserMarkerItem*> _usermarkers;

	bool _showAnnotationBorders;
	bool _showUserMarkers;

	QTimer *_previewClearTimer;
	QTimer *_animTickTimer;

	QString _title;
};

}

#endif

