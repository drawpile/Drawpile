// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_VIEW_CANVASSCENE_H
#define DESKTOP_VIEW_CANVASSCENE_H
#include "libclient/canvas/selection.h"
#include <QElapsedTimer>
#include <QGraphicsScene>
#include <QHash>

class QTransform;

namespace canvas {
class CanvasModel;
}

namespace drawdance {
class AnnotationList;
}

namespace drawingboard {
class AnnotationItem;
class BaseItem;
class CatchupItem;
#ifdef HAVE_EMULATED_BITMAP_CURSOR
class CursorItem;
#endif
class LaserTrailItem;
class NoticeItem;
class SelectionItem;
class ToggleItem;
class UserMarkerItem;
}

namespace view {

class CanvasScene final : public QGraphicsScene {
	Q_OBJECT
	using AnnotationItem = drawingboard::AnnotationItem;
	using BaseItem = drawingboard::BaseItem;
	using CatchupItem = drawingboard::CatchupItem;
#ifdef HAVE_EMULATED_BITMAP_CURSOR
	using CursorItem = drawingboard::CursorItem;
#endif
	using LaserTrailItem = drawingboard::LaserTrailItem;
	using SelectionItem = drawingboard::SelectionItem;
	using NoticeItem = drawingboard::NoticeItem;
	using ToggleItem = drawingboard::ToggleItem;
	using UserMarkerItem = drawingboard::UserMarkerItem;

public:
	explicit CanvasScene(QObject *parent = nullptr);

	void setCanvasModel(canvas::CanvasModel *canvasModel);
	void setCanvasTransform(const QTransform &canvasTransform);

	void setCanvasVisible(bool canvasVisible);
	void setShowAnnotations(bool showAnnotations);
	void setShowAnnotationBorders(bool showAnnotationBorders);
	void setShowUserMarkers(bool showUserMarkers);
	void setShowUserNames(bool showUserNames);
	void setShowUserLayers(bool showUserLayers);
	void setShowUserAvatars(bool showUserAvatars);
	void setShowOwnUserMarker(bool showOwnUserMarker);
	void setShowLaserTrails(bool showLaserTrails);
	void setShowToggleItems(bool showToggleItems);

#ifdef HAVE_EMULATED_BITMAP_CURSOR
	void setCursorOnCanvas(bool cursorOnCanvas);
	void setCursor(const QCursor &cursor);
	void setCursorPos(const QPointF &pos);
#endif

	void setActiveAnnotation(int annotationId);
	AnnotationItem *getAnnotationItem(int annotationId);

	void setNotificationBarHeight(int height);

	bool showTransformNotice(const QString &text);

	bool showLockNotice(const QString &text);
	bool hideLockNotice();

	bool hasCatchup() const;
	void setCatchupProgress(int percent);

	int checkHover(const QPointF &scenePos, bool *outWasHovering = nullptr);
	void removeHover();

signals:
	void annotationDeleted(int id);

private:
	static constexpr qreal NOTICE_OFFSET = 16.0;
	static constexpr qreal NOTICE_PERSIST = 1.0;

	void addSceneItem(BaseItem *item);

	void onSceneRectChanged();

	void onUserJoined(int id);
	void
	onCursorMoved(unsigned int flags, int userId, int layerId, int x, int y);
	void onLaserTrail(int userId, int persistence, const QColor &color);

	void onSelectionChanged(canvas::Selection *sel);

	void onAnnotationsChanged(const drawdance::AnnotationList &al);
	void onPreviewAnnotation(int annotationId, const QRect &shape);

	void setTransformNoticePosition();
	void setLockNoticePosition();
	void setCatchupPosition();
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
	bool m_showOwnUserMarker = false;
	bool m_showLaserTrails = true;
	QHash<int, UserMarkerItem *> m_userMarkers;
	QHash<int, LaserTrailItem *> m_activeLaserTrails;

	SelectionItem *m_selection = nullptr;

#ifdef HAVE_EMULATED_BITMAP_CURSOR
	CursorItem *m_cursorItem;
#endif

	qreal m_topOffset = 0.0;
	NoticeItem *m_transformNotice = nullptr;
	NoticeItem *m_lockNotice = nullptr;

	CatchupItem *m_catchup = nullptr;

	QVector<ToggleItem *> m_toggleItems;

	QElapsedTimer m_animationElapsedTimer;
};

}

#endif
