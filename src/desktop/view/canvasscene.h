// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_VIEW_CANVASSCENE_H
#define DESKTOP_VIEW_CANVASSCENE_H
#include "libclient/canvas/selectionmodel.h"
#include <QColor>
#include <QElapsedTimer>
#include <QGraphicsScene>
#include <QHash>

class QPainterPath;
class QTransform;

namespace canvas {
class CanvasModel;
}

namespace drawdance {
class AnnotationList;
class SelectionSet;
}

namespace drawingboard {
class AnchorLineItem;
class AnnotationItem;
class BaseItem;
class CatchupItem;
class ColorPickItem;
#ifdef HAVE_EMULATED_BITMAP_CURSOR
class CursorItem;
#endif
class LaserTrailItem;
class MaskPreviewItem;
class NoticeItem;
class OutlineItem;
class PathPreviewItem;
class SelectionItem;
class ToggleItem;
class TransformItem;
class UserMarkerItem;
}

namespace view {

class CanvasScene final : public QGraphicsScene {
	Q_OBJECT
	using AnchorLineItem = drawingboard::AnchorLineItem;
	using AnnotationItem = drawingboard::AnnotationItem;
	using BaseItem = drawingboard::BaseItem;
	using CatchupItem = drawingboard::CatchupItem;
	using ColorPickItem = drawingboard::ColorPickItem;
#ifdef HAVE_EMULATED_BITMAP_CURSOR
	using CursorItem = drawingboard::CursorItem;
#endif
	using LaserTrailItem = drawingboard::LaserTrailItem;
	using MaskPreviewItem = drawingboard::MaskPreviewItem;
	using NoticeItem = drawingboard::NoticeItem;
	using OutlineItem = drawingboard::OutlineItem;
	using PathPreviewItem = drawingboard::PathPreviewItem;
	using SelectionItem = drawingboard::SelectionItem;
	using ToggleItem = drawingboard::ToggleItem;
	using TransformItem = drawingboard::TransformItem;
	using UserMarkerItem = drawingboard::UserMarkerItem;

public:
	explicit CanvasScene(bool outline, QObject *parent = nullptr);

	void setCanvasModel(canvas::CanvasModel *canvasModel);
	void setCanvasTransform(const QTransform &canvasTransform);
	void setZoom(qreal zoom);

	void setCanvasVisible(bool canvasVisible);
	void setShowAnnotations(bool showAnnotations);
	void setShowAnnotationBorders(bool showAnnotationBorders);
	void setShowUserMarkers(bool showUserMarkers);
	void setShowUserNames(bool showUserNames);
	void setShowUserLayers(bool showUserLayers);
	void setShowUserAvatars(bool showUserAvatars);
	void setEvadeUserCursors(bool evadeUserCursors);
	void setShowOwnUserMarker(bool showOwnUserMarker);
	void setShowLaserTrails(bool showLaserTrails);
	void setShowToggleItems(bool showToggleItems);
	void setShowSelectionMask(bool showSelectionMask);
	void setUserMarkerPersistence(int userMarkerPersistence);

	void setAnchorLine(const QVector<QPointF> &points, int activeIndex);
	void setAnchorLineActiveIndex(int activeIndex);
	void setMaskPreview(const QPoint &pos, const QImage &mask);
	void setPathPreview(const QPainterPath &path);

	bool isCursorOnCanvas() const { return m_cursorOnCanvas; }
	void setCursorOnCanvas(bool cursorOnCanvas);
	const QPointF &cursorPos() const { return m_cursorPos; }
	void setCursorPos(const QPointF &cursorPos);
	void setOutline(
		bool visibleInMode, const QPointF &pos, qreal rotation,
		qreal outlineSize, qreal outlineWidth, bool square);

	void setForegroundColor(const QColor &foregroundColor);
	void setComparisonColor(const QColor &comparisonColor);
	bool showColorPick(int source, const QPointF &pos);
	void hideColorPick();
	void setColorPickVisibility(int colorPickVisibility);
	bool setColorAdjust(bool visible, const QPointF &pos);

#ifdef HAVE_EMULATED_BITMAP_CURSOR
	void setCursor(const QCursor &cursor);
#endif

	void setActiveAnnotation(int annotationId);
	AnnotationItem *getAnnotationItem(int annotationId);

	void setSelectionIgnored(bool selectionIgnored);

	void setTransformToolState(int mode, int handle, bool dragging);

	void setNotificationBarHeight(int height);

	bool showTransformNotice(const QString &text);

	bool showLockNotice(const QString &text);
	bool hideLockNotice();

	void showPopupNotice(const QString &text);

	void setToolNotice(const QString &text);

	bool hasCatchup() const;
	void setCatchupProgress(int percent);
	void setStreamResetProgress(int percent);
	static QString getStreamResetProgressText(int percent);

	int checkHover(const QPointF &scenePos, bool *outWasHovering = nullptr);
	void removeHover();

signals:
	void annotationDeleted(int id);

private:
	static constexpr qreal NOTICE_OFFSET = 16.0;
	static constexpr qreal NOTICE_PERSIST = 1.0;
	static constexpr qreal POPUP_PERSIST = 3.0;

	void addSceneItem(BaseItem *item);

	void onSceneRectChanged();

	void onUserJoined(int id);
	void
	onCursorMoved(unsigned int flags, int userId, int layerId, int x, int y);
	void onLaserTrail(int userId, int persistence, const QColor &color);

	void setSelection(const QSharedPointer<canvas::SelectionMask> &mask);
	void onTransformChanged();

	void onAnnotationsChanged(const drawdance::AnnotationList &al);
	void onPreviewAnnotation(int annotationId, const QRect &shape);

	void setTransformNoticePosition();
	void setLockNoticePosition();
	void setToolNoticePosition();
	void setPopupNoticePosition();
	void setCatchupPosition();
	void setStreamResetNoticePosition();
	void setTogglePositions();

	void advanceAnimations();

	canvas::CanvasModel *m_canvasModel = nullptr;

	QGraphicsItemGroup *m_canvasGroup;

	bool m_canvasVisible = true;
	bool m_showAnnotations = true;
	bool m_showAnnotationBorders = false;
	bool m_showUserMarkers = true;
	bool m_showUserNames = true;
	bool m_showUserLayers = true;
	bool m_showUserAvatars = true;
	bool m_evadeUserCursors = true;
	bool m_showOwnUserMarker = false;
	bool m_showLaserTrails = true;
	bool m_cursorOnCanvas = false;
	bool m_selectionIgnored = false;
	bool m_showSelectionMask = false;
	int m_userMarkerPersistence = 1000;
	int m_colorPickVisibility;
	qreal m_zoom = 1.0;
	QPointF m_cursorPos;
	QHash<int, UserMarkerItem *> m_userMarkers;
	QHash<int, LaserTrailItem *> m_activeLaserTrails;

	AnchorLineItem *m_anchorLine = nullptr;
	MaskPreviewItem *m_maskPreview = nullptr;
	PathPreviewItem *m_pathPreview = nullptr;
	SelectionItem *m_selection = nullptr;
	TransformItem *m_transform = nullptr;
	OutlineItem *m_outline = nullptr;
	ColorPickItem *m_colorPick = nullptr;
	QColor m_foregroundColor;
	QColor m_comparisonColor;

#ifdef HAVE_EMULATED_BITMAP_CURSOR
	CursorItem *m_cursorItem;
#endif

	qreal m_topOffset = 0.0;
	NoticeItem *m_transformNotice = nullptr;
	NoticeItem *m_lockNotice = nullptr;
	NoticeItem *m_toolNotice = nullptr;
	NoticeItem *m_popupNotice = nullptr;

	CatchupItem *m_catchup = nullptr;
	NoticeItem *m_streamResetNotice = nullptr;

	QVector<ToggleItem *> m_toggleItems;

	QElapsedTimer m_animationElapsedTimer;
};

}

#endif
