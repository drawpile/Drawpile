// SPDX-License-Identifier: GPL-3.0-or-later

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
class CanvasScene final : public QGraphicsScene {
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

	void userCursorMoved(
		unsigned int flags, uint8_t userId, uint16_t layerId, int x, int y);

	void laserTrail(uint8_t userId, int persistence, const QColor &color);

	void annotationsChanged(const drawdance::AnnotationList &al);
	void previewAnnotation(int id, const QRect &shape);

private:
	//! The actual canvas model
	canvas::CanvasModel *m_model;

	//! The item that shows the canvas pixel content
	CanvasItem *m_canvasItem;

	//! Active laser pointer trail item (per user)
	QHash<uint8_t, LaserTrailItem *> m_activeLaserTrail;

	//! User cursor items
	QHash<uint8_t, UserMarkerItem *> m_usermarkers;

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
