/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2021 Calle Laakkonen

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

namespace canvas {
	class CanvasModel;
	class Selection;
}

namespace drawdance {
	class AnnotationList;
}

//! Drawing board related classes
namespace drawingboard {

class CanvasItem;
class AnnotationItem;
class SelectionItem;
class UserMarkerItem;
class LaserTrailItem;

/**
 * @brief The drawing board scene
 */
class CanvasScene final : public QGraphicsScene
{
	Q_OBJECT

public:
	//! Margin around the image to make working near corners easier
	static const int MARGIN = 900;

	explicit CanvasScene(QObject *parent);

	//! Clear the canvas and assign a new model to it
	void initCanvas(canvas::CanvasModel *model);

	//! Is there an image on the drawing board
	bool hasImage() const { return m_model != nullptr; }

	//! Are annotation borders shown?
	bool showAnnotationBorders() const { return m_showAnnotationBorders; }

	//! Show/hide annotations
	void showAnnotations(bool show);

	//! Get the current canvas model
	canvas::CanvasModel *model() const { return m_model; }

	//! Get an annotation item
	AnnotationItem *getAnnotationItem(int id);

public slots:
	//! Show annotation borders
	void showAnnotationBorders(bool show);

	//! Show/hide remote cursor markers
	void showUserMarkers(bool show);

	//! Show user names in cursor markers
	void showUserNames(bool show);

	//! Show layer selection in cursor marker
	void showUserLayers(bool show);

	//! Show avatars in cursor marker
	void showUserAvatars(bool show);

	//! Show/hide laser pointer trails
	void showLaserTrails(bool show);

	//! Select the currently active/highlighted annotation
	void setActiveAnnotation(int id);

	//! Reveal the canvas item
	void showCanvas();

	//! Hide canvas item
	void hideCanvas();

	void canvasViewportChanged(const QPolygonF &viewport);

signals:
	//! Canvas size has just changed
	void canvasResized(int xoffset, int yoffset, const QSize &oldSize);

	//! An annotation item was just deleted
	void annotationDeleted(int id);

private slots:
	void onSelectionChanged(canvas::Selection *sel);
	void handleCanvasResize(int xoffset, int yoffset, const QSize &oldsize);
	void advanceAnimations();

	void userCursorMoved(uint8_t userId, uint16_t layerId, int x, int y);
	void laserTrail(uint8_t userId, int persistence, const QColor &color);

	void annotationsChanged(const drawdance::AnnotationList &al);
	void previewAnnotation(int id, const QRect &shape);

private:
	//! The actual canvas model
	canvas::CanvasModel *m_model;

	//! The item that shows the canvas pixel content
	CanvasItem *m_canvasItem;

	//! Active laser pointer trail item (per user)
	QHash<uint8_t, LaserTrailItem*> m_activeLaserTrail;

	//! User cursor items
	QHash<uint8_t, UserMarkerItem*> m_usermarkers;

	//! Current selection
	SelectionItem *m_selection;

	bool m_showAnnotationBorders;
	bool m_showAnnotations;
	bool m_showUserMarkers;
	bool m_showUserNames;
	bool m_showUserLayers;
	bool m_showUserAvatars;
	bool m_showLaserTrails;
};

}

#endif

