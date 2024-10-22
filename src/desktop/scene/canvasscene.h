// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CANVAS_SCENE_H
#define CANVAS_SCENE_H
#include "desktop/scene/toggleitem.h"
#include <QColor>
#include <QGraphicsScene>
#include <QHash>

namespace canvas {
class CanvasModel;
}

namespace drawdance {
class AnnotationList;
}

//! Drawing board related classes
namespace drawingboard {

class AnnotationItem;
class CanvasItem;
class CatchupItem;
class ColorPickItem;
class LaserTrailItem;
class MaskPreviewItem;
class NoticeItem;
class OutlineItem;
class PathPreviewItem;
class SelectionItem;
class TransformItem;
class UserMarkerItem;
#ifdef HAVE_EMULATED_BITMAP_CURSOR
class CursorItem;
#endif

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
	void setZoom(qreal zoom);

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

	void setToolNotice(const QString &text);

	ToggleItem::Action
	checkHover(const QPointF &scenePos, bool *outWasHovering = nullptr);

	void removeHover();

	//! Show/hide annotations
	void showAnnotations(bool show);

	//! Get the current canvas model
	canvas::CanvasModel *model() const { return m_model; }

	//! Get an annotation item
	AnnotationItem *getAnnotationItem(int id);

	void setSelectionIgnored(bool selectionIgnored);
	void setShowSelectionMask(bool showSelectionMask);

	void setTransformToolState(int mode, int handle, bool dragging);

	void setShowOwnUserMarker(bool showOwnUserMarker);
	void setUserMarkerPersistence(int userMarkerPersistence);

	bool hasCatchup() const { return m_catchup != nullptr; }

	void setMaskPreview(const QPoint &pos, const QImage &mask);
	void setPathPreview(const QPainterPath &path);

	void setOutline(qreal size, qreal width);
	void setOutlineTransform(const QPointF &pos, qreal angle);
	void setOutlineSquare(bool square);
	void setOutlineWidth(qreal width);
	void setOutlineVisibleInMode(bool visibleInMode);
	void setComparisonColor(const QColor &comparisonColor);
	bool setColorPick(int source, const QPointF &pos, const QColor &color);
	void setColorPickVisibility(int colorPickVisibility);
	bool isCursorOnCanvas() const { return m_cursorOnCanvas; }
	void setCursorOnCanvas(bool onCanvas);
	const QPointF &cursorPos() const { return m_cursorPos; }
	void setCursorPos(const QPointF &pos);
#ifdef HAVE_EMULATED_BITMAP_CURSOR
	void setCursor(const QCursor &cursor);
#endif

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

	void setEvadeUserCursors(bool evadeUserCursors);

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

	void setStreamResetProgress(int percent);

signals:
	//! Canvas size has just changed
	void canvasResized(int xoffset, int yoffset, const QSize &oldSize);

	//! An annotation item was just deleted
	void annotationDeleted(int id);

private slots:
	void onUserJoined(int id, const QString &name);
	void handleCanvasResize(
		const QSize newSize, const QPoint &offset, const QSize &oldSize);
	void advanceAnimations();

	void
	userCursorMoved(unsigned int flags, int userId, int layerId, int x, int y);

	void laserTrail(int userId, int persistence, const QColor &color);

	void annotationsChanged(const drawdance::AnnotationList &al);
	void previewAnnotation(int id, const QRect &shape);

	void setSelection(bool valid, const QRect &bounds, const QImage &mask);
	void onTransformChanged();

private:
	static constexpr qreal NOTICE_OFFSET = 16.0;
	static constexpr qreal NOTICE_PERSIST = 1.0;
	static constexpr qreal TOOL_NOTICE_OFFSET = 8.0;

	void setTransformNoticePosition();
	void setLockNoticePosition();
	void setToolNoticePosition();
	void setCatchupPosition();
	void setStreamResetNoticePosition();

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

	MaskPreviewItem *m_maskPreview;
	PathPreviewItem *m_pathPreview;
	SelectionItem *m_selection;
	TransformItem *m_transform;

	qreal m_topOffset = 0.0;
	NoticeItem *m_transformNotice;
	NoticeItem *m_lockNotice;
	NoticeItem *m_toolNotice;
	CatchupItem *m_catchup;
	NoticeItem *m_streamResetNotice;
	QVector<ToggleItem *> m_toggleItems;

	OutlineItem *m_outlineItem;
	ColorPickItem *m_colorPick = nullptr;
	QColor m_comparisonColor;
#ifdef HAVE_EMULATED_BITMAP_CURSOR
	CursorItem *m_cursorItem;
#endif

	bool m_showAnnotationBorders;
	bool m_showAnnotations;
	bool m_showUserMarkers;
	bool m_showUserNames;
	bool m_showUserLayers;
	bool m_showUserAvatars;
	bool m_evadeUserCursors;
	bool m_showLaserTrails;
	bool m_showOwnUserMarker;
	bool m_cursorOnCanvas;
	bool m_selectionIgnored;
	bool m_showSelectionMask;
	int m_userMarkerPersistence;
	int m_colorPickVisibility;
	qreal m_zoom = 1.0;
	QPointF m_cursorPos;

	QRectF m_sceneBounds;
};

}

#endif
