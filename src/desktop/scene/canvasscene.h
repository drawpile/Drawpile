// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CANVAS_SCENE_H
#define CANVAS_SCENE_H

#include "desktop/scene/toggleitem.h"
#include <QGraphicsScene>
#include <QHash>

namespace canvas {
class CanvasModel;
class Selection;
}

namespace drawdance {
class AnnotationList;
}

//! Drawing board related classes
namespace drawingboard {

class AnnotationItem;
class CanvasItem;
class CatchupItem;
class LaserTrailItem;
class NoticeItem;
class SelectionItem;
class UserMarkerItem;

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

	QTransform canvasTransform() const;
	void setCanvasTransform(const QTransform &matrix);

	void setCanvasPixelGrid(bool pixelGrid);

	QRectF canvasBounds() const;

	//! Is there an image on the drawing board
	bool hasImage() const { return m_model != nullptr; }

	void setSceneBounds(const QRectF &sceneBounds);

	// Move HUD notices down when a reconnect or reset notification is shown.
	void setNotificationBarHeight(int height);

	void showTransformNotice(const QString &text);

	void showLockNotice(const QString &text);
	void hideLockNotice();

	ToggleItem::Action
	checkHover(const QPointF &scenePos, bool *outWasHovering = nullptr);

	void removeHover();

	//! Are annotation borders shown?
	bool showAnnotationBorders() const { return m_showAnnotationBorders; }

	//! Show/hide annotations
	void showAnnotations(bool show);

	//! Get the current canvas model
	canvas::CanvasModel *model() const { return m_model; }

	//! Get an annotation item
	AnnotationItem *getAnnotationItem(int id);

	void setShowOwnUserMarker(bool show) { m_showOwnUserMarker = show; }

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

	void showToggleItems(bool show);

	//! Select the currently active/highlighted annotation
	void setActiveAnnotation(int id);

	//! Reveal the canvas item
	void showCanvas();

	//! Hide canvas item
	void hideCanvas();

	void canvasViewportChanged(const QPolygonF &viewport);

	void setCatchupProgress(int percent);

signals:
	//! Canvas size has just changed
	void canvasResized(int xoffset, int yoffset, const QSize &oldSize);

	//! An annotation item was just deleted
	void annotationDeleted(int id);

private slots:
	void onSelectionChanged(canvas::Selection *sel);
	void onUserJoined(int id, const QString &name);
	void handleCanvasResize(int xoffset, int yoffset, const QSize &oldsize);
	void advanceAnimations();

	void userCursorMoved(
		unsigned int flags, uint8_t userId, uint16_t layerId, int x, int y);

	void laserTrail(uint8_t userId, int persistence, const QColor &color);

	void annotationsChanged(const drawdance::AnnotationList &al);
	void previewAnnotation(int id, const QRect &shape);

private:
	static constexpr qreal NOTICE_OFFSET = 16.0;
	static constexpr qreal NOTICE_PERSIST = 1.0;

	void setTransformNoticePosition();
	void setLockNoticePosition();
	void setCatchupPosition();

	//! The actual canvas model
	canvas::CanvasModel *m_model;

	//! View wrapper for relative positioning
	QGraphicsItemGroup *m_group;

	//! The item that shows the canvas pixel content
	CanvasItem *m_canvasItem;

	//! Active laser pointer trail item (per user)
	QHash<uint8_t, LaserTrailItem *> m_activeLaserTrail;

	//! User cursor items
	QHash<uint8_t, UserMarkerItem *> m_usermarkers;

	//! Current selection
	SelectionItem *m_selection;

	qreal m_topOffset = 0.0;
	NoticeItem *m_transformNotice;
	NoticeItem *m_lockNotice;
	CatchupItem *m_catchup;
	QVector<ToggleItem *> m_toggleItems;

	bool m_showAnnotationBorders;
	bool m_showAnnotations;
	bool m_showUserMarkers;
	bool m_showUserNames;
	bool m_showUserLayers;
	bool m_showUserAvatars;
	bool m_showLaserTrails;
	bool m_showOwnUserMarker;

	QRectF m_sceneBounds;
};

}

#endif
