// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/user_cursors.h>
}
#include "desktop/scene/anchorlineitem.h"
#include "desktop/scene/annotationitem.h"
#include "desktop/scene/colorpickitem.h"
#include "desktop/scene/lasertrailitem.h"
#include "desktop/scene/maskpreviewitem.h"
#include "desktop/scene/outlineitem.h"
#include "desktop/scene/pathpreviewitem.h"
#include "desktop/scene/selectionitem.h"
#include "desktop/scene/transformitem.h"
#include "desktop/scene/usermarkeritem.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/view/canvasscene.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/layerlist.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/canvas/selectionmodel.h"
#include "libclient/canvas/transformmodel.h"
#include "libclient/canvas/userlist.h"
#include <QGraphicsItemGroup>
#include <QPainterPath>
#include <QTimer>
#include <QTransform>
#include <utility>
#ifdef HAVE_EMULATED_BITMAP_CURSOR
#	include "desktop/scene/cursoritem.h"
#endif

namespace view {

CanvasScene::CanvasScene(bool outline, QObject *parent)
	: QGraphicsScene(parent)
	, m_hud(new HudHandler(this, this))
	, m_colorPickVisibility(ColorPickItem::defaultVisibility())
{
	m_canvasGroup = new QGraphicsItemGroup;
	addItem(m_canvasGroup);

	if(outline) {
		m_outline = new OutlineItem(m_canvasGroup);
		m_outline->setUpdateSceneOnRefresh(true);
	}

#ifdef HAVE_EMULATED_BITMAP_CURSOR
	m_cursorItem = new CursorItem;
	addSceneItem(m_cursorItem);
#endif

	connect(
		this, &CanvasScene::sceneRectChanged, m_hud,
		&HudHandler::updateItemsPositions, Qt::DirectConnection);

	QTimer *animationTimer = new QTimer(this);
	animationTimer->setTimerType(Qt::CoarseTimer);
	animationTimer->setInterval(20);
	connect(
		animationTimer, &QTimer::timeout, this,
		&CanvasScene::advanceAnimations);
	m_animationElapsedTimer.start();
	animationTimer->start();
}

void CanvasScene::setCanvasModel(canvas::CanvasModel *canvasModel)
{
	m_canvasModel = canvasModel;

	connect(
		canvasModel, &canvas::CanvasModel::userJoined, this,
		&CanvasScene::onUserJoined);
	connect(
		canvasModel, &canvas::CanvasModel::laserTrail, this,
		&CanvasScene::onLaserTrail);
	connect(
		m_canvasModel, &canvas::CanvasModel::previewAnnotationRequested, this,
		&CanvasScene::onPreviewAnnotation);
	connect(
		m_canvasModel->selection(), &canvas::SelectionModel::selectionChanged,
		this, &CanvasScene::setSelection);
	connect(
		m_canvasModel->transform(), &canvas::TransformModel::transformChanged,
		this, &CanvasScene::onTransformChanged);

	canvas::PaintEngine *pe = canvasModel->paintEngine();
	connect(
		pe, &canvas::PaintEngine::cursorMoved, this,
		&CanvasScene::onCursorMoved);
	connect(
		pe, &canvas::PaintEngine::annotationsChanged, this,
		&CanvasScene::onAnnotationsChanged);

	for(QGraphicsItem *item : m_canvasGroup->childItems()) {
		if(item->type() == AnnotationItem::Type ||
		   item->type() == UserMarkerItem::Type ||
		   item->type() == LaserTrailItem::Type) {
			delete item;
		}
	}
	m_userMarkers.clear();
	m_activeLaserTrails.clear();

	setSelection(canvasModel->selection()->mask());
	onTransformChanged();

	update();
}

void CanvasScene::setCanvasTransform(const QTransform &canvasTransform)
{
	m_canvasGroup->setTransform(canvasTransform);
}

void CanvasScene::setZoom(qreal zoom)
{
	if(zoom != m_zoom) {
		m_zoom = zoom;
		for(QGraphicsItem *item : m_canvasGroup->childItems()) {
			AnnotationItem *ai = qgraphicsitem_cast<AnnotationItem *>(item);
			if(ai) {
				ai->setZoom(zoom);
			}
		}
		if(m_anchorLine) {
			m_anchorLine->setZoom(zoom);
		}
		if(m_pathPreview) {
			m_pathPreview->setZoom(zoom);
		}
		if(m_selection) {
			m_selection->setZoom(zoom);
		}
		if(m_transform) {
			m_transform->setZoom(zoom);
		}
	}
}

void CanvasScene::setCanvasVisible(bool canvasVisible)
{
	if(canvasVisible != m_canvasVisible) {
		m_canvasVisible = canvasVisible;
		m_canvasGroup->setVisible(canvasVisible);
		update(m_canvasGroup->sceneBoundingRect());
	}
}

void CanvasScene::setShowAnnotations(bool showAnnotations)
{
	if(showAnnotations != m_showAnnotations) {
		m_showAnnotations = showAnnotations;
		for(QGraphicsItem *item : m_canvasGroup->childItems()) {
			if(item->type() == AnnotationItem::Type) {
				item->setVisible(showAnnotations);
			}
		}
	}
}

void CanvasScene::setShowAnnotationBorders(bool showAnnotationBorders)
{
	if(showAnnotationBorders != m_showAnnotationBorders) {
		m_showAnnotationBorders = showAnnotationBorders;
		for(QGraphicsItem *item : m_canvasGroup->childItems()) {
			AnnotationItem *ai = qgraphicsitem_cast<AnnotationItem *>(item);
			if(ai) {
				ai->setShowBorder(showAnnotationBorders);
			}
		}
	}
}

void CanvasScene::setShowUserMarkers(bool showUserMarkers)
{
	if(showUserMarkers != m_showUserMarkers) {
		m_showUserMarkers = showUserMarkers;
		if(!showUserMarkers) {
			for(UserMarkerItem *item : std::as_const(m_userMarkers)) {
				item->hide();
			}
		}
	}
}

void CanvasScene::setShowUserNames(bool showUserNames)
{
	if(showUserNames != m_showUserNames) {
		m_showUserNames = showUserNames;
		for(UserMarkerItem *item : std::as_const(m_userMarkers)) {
			item->setShowText(showUserNames);
		}
	}
}

void CanvasScene::setShowUserLayers(bool showUserLayers)
{
	if(showUserLayers != m_showUserLayers) {
		m_showUserLayers = showUserLayers;
		for(UserMarkerItem *item : std::as_const(m_userMarkers)) {
			item->setShowSubtext(showUserLayers);
		}
	}
}

void CanvasScene::setShowUserAvatars(bool showUserAvatars)
{
	if(showUserAvatars != m_showUserAvatars) {
		m_showUserAvatars = showUserAvatars;
		for(UserMarkerItem *item : std::as_const(m_userMarkers)) {
			item->setShowAvatar(showUserAvatars);
		}
	}
}

void CanvasScene::setEvadeUserCursors(bool evadeUserCursors)
{
	if(evadeUserCursors != m_evadeUserCursors) {
		m_evadeUserCursors = evadeUserCursors;
		for(UserMarkerItem *item : std::as_const(m_userMarkers)) {
			item->setEvadeCursor(evadeUserCursors);
		}
	}
}

void CanvasScene::setShowOwnUserMarker(bool showOwnUserMarker)
{
	if(showOwnUserMarker != m_showOwnUserMarker) {
		m_showOwnUserMarker = showOwnUserMarker;
		for(UserMarkerItem *item : std::as_const(m_userMarkers)) {
			item->setCursorPosValid(m_cursorOnCanvas && !showOwnUserMarker);
		}
	}
}

void CanvasScene::setShowLaserTrails(bool showLaserTrails)
{
	if(showLaserTrails != m_showLaserTrails) {
		m_showLaserTrails = showLaserTrails;
		if(!showLaserTrails) {
			m_activeLaserTrails.clear();
			for(QGraphicsItem *item : m_canvasGroup->childItems()) {
				if(item->type() == LaserTrailItem::Type) {
					delete item;
				}
			}
		}
	}
}

void CanvasScene::setShowSelectionMask(bool showSelectionMask)
{
	m_showSelectionMask = showSelectionMask;
	if(m_selection) {
		m_selection->setShowMask(showSelectionMask);
	}
}

void CanvasScene::setUserMarkerPersistence(int userMarkerPersistence)
{
	if(userMarkerPersistence != m_userMarkerPersistence) {
		m_userMarkerPersistence = userMarkerPersistence;
		for(UserMarkerItem *item : std::as_const(m_userMarkers)) {
			item->setPersistence(userMarkerPersistence);
		}
	}
}

void CanvasScene::setAnchorLine(const QVector<QPointF> &points, int activeIndex)
{
	if(points.isEmpty()) {
		delete m_anchorLine;
		m_anchorLine = nullptr;
	} else if(m_anchorLine) {
		m_anchorLine->setPoints(points, activeIndex);
	} else {
		m_anchorLine =
			new AnchorLineItem(points, activeIndex, m_zoom, m_canvasGroup);
		m_anchorLine->setUpdateSceneOnRefresh(true);
	}
}

void CanvasScene::setAnchorLineActiveIndex(int activeIndex)
{
	if(m_anchorLine) {
		m_anchorLine->setActiveIndex(activeIndex);
	}
}

void CanvasScene::setMaskPreview(const QPoint &pos, const QImage &mask)
{
	if(mask.isNull() || mask.size().isEmpty()) {
		delete m_maskPreview;
		m_maskPreview = nullptr;
	} else if(m_maskPreview) {
		m_maskPreview->updatePosition(QPointF(pos));
		m_maskPreview->setMask(mask);
	} else {
		m_maskPreview = new MaskPreviewItem(mask, m_canvasGroup);
		m_maskPreview->setUpdateSceneOnRefresh(true);
		m_maskPreview->updatePosition(QPointF(pos));
	}
}

void CanvasScene::setPathPreview(const QPainterPath &path)
{
	if(path.isEmpty()) {
		delete m_pathPreview;
		m_pathPreview = nullptr;
	} else if(m_pathPreview) {
		m_pathPreview->setPath(path);
	} else {
		m_pathPreview = new PathPreviewItem(path, m_zoom, m_canvasGroup);
		m_pathPreview->setUpdateSceneOnRefresh(true);
	}
}

void CanvasScene::setCursorOnCanvas(bool cursorOnCanvas)
{
	if(cursorOnCanvas != m_cursorOnCanvas) {
		m_cursorOnCanvas = cursorOnCanvas;
		if(m_outline) {
			m_outline->setOnCanvas(cursorOnCanvas);
		}
#ifdef HAVE_EMULATED_BITMAP_CURSOR
		m_cursorItem->setOnCanvas(cursorOnCanvas);
#endif
		for(UserMarkerItem *item : std::as_const(m_userMarkers)) {
			item->setCursorPosValid(cursorOnCanvas && !m_showOwnUserMarker);
		}
	}
}

void CanvasScene::setCursorPos(const QPointF &cursorPos)
{
	if(cursorPos != m_cursorPos) {
		m_cursorPos = cursorPos;
#ifdef HAVE_EMULATED_BITMAP_CURSOR
		m_cursorItem->updatePosition(cursorPos);
#endif
		for(UserMarkerItem *item : std::as_const(m_userMarkers)) {
			item->setCursorPos(cursorPos);
		}
	}
}

void CanvasScene::setOutline(
	bool visibleInMode, const QPointF &outlinePos, qreal rotation,
	qreal outlineSize, qreal outlineWidth, bool square)
{
	if(m_outline) {
		m_outline->setVisibleInMode(visibleInMode);
		m_outline->updatePosition(outlinePos);
		m_outline->updateRotation(rotation);
		m_outline->setOutline(outlineSize, outlineWidth);
		m_outline->setSquare(square);
	}
}

void CanvasScene::setForegroundColor(const QColor &foregroundColor)
{
	m_foregroundColor = foregroundColor;
	if(m_colorPick) {
		m_colorPick->setColor(foregroundColor);
	}
}

void CanvasScene::setComparisonColor(const QColor &comparisonColor)
{
	m_comparisonColor = comparisonColor;
	if(m_colorPick) {
		m_colorPick->setComparisonColor(comparisonColor);
	}
}

bool CanvasScene::showColorPick(int source, const QPointF &pos)
{
	if(ColorPickItem::shouldShow(source, m_colorPickVisibility)) {
		if(!m_colorPick) {
			m_colorPick =
				new ColorPickItem(m_foregroundColor, m_comparisonColor);
			addSceneItem(m_colorPick);
		}
		m_colorPick->updatePosition(pos);
		return true;
	} else {
		return m_colorPick;
	}
}

void CanvasScene::hideColorPick()
{
	delete m_colorPick;
	m_colorPick = nullptr;
}

void CanvasScene::setColorPickVisibility(int colorPickVisibility)
{
	m_colorPickVisibility = colorPickVisibility;
}

bool CanvasScene::setColorAdjust(bool visible, const QPointF &pos)
{
	if(visible) {
		if(!m_colorPick) {
			m_colorPick =
				new ColorPickItem(m_foregroundColor, m_comparisonColor);
			addSceneItem(m_colorPick);
		}
		m_colorPick->updatePosition(pos);
		return true;
	} else {
		delete m_colorPick;
		m_colorPick = nullptr;
		return false;
	}
}

#ifdef HAVE_EMULATED_BITMAP_CURSOR
void CanvasScene::setCursor(const QCursor &cursor)
{
	m_cursorItem->setCursor(cursor);
}
#endif

void CanvasScene::setActiveAnnotation(int annotationId)
{
	for(QGraphicsItem *item : m_canvasGroup->childItems()) {
		AnnotationItem *ai = qgraphicsitem_cast<AnnotationItem *>(item);
		if(ai) {
			ai->setHighlight(ai->id() == annotationId);
		}
	}
}

drawingboard::AnnotationItem *CanvasScene::getAnnotationItem(int annotationId)
{
	for(QGraphicsItem *item : m_canvasGroup->childItems()) {
		AnnotationItem *ai = qgraphicsitem_cast<AnnotationItem *>(item);
		if(ai && ai->id() == annotationId) {
			return ai;
		}
	}
	return nullptr;
}

void CanvasScene::setSelectionIgnored(bool selectionIgnored)
{
	m_selectionIgnored = selectionIgnored;
	if(m_selection) {
		m_selection->setIgnored(selectionIgnored);
	}
}

void CanvasScene::setTransformToolState(int mode, int handle, bool dragging)
{
	if(m_transform) {
		m_transform->setToolState(mode, handle, dragging);
	}
}

QRectF CanvasScene::hudSceneRect() const
{
	return sceneRect();
}

void CanvasScene::hudAddItem(BaseItem *item)
{
	addSceneItem(item);
}

void CanvasScene::hudRemoveItem(BaseItem *item)
{
	removeSceneItem(item);
}

void CanvasScene::addSceneItem(BaseItem *item)
{
	item->setUpdateSceneOnRefresh(true);
	addItem(item);
}

void CanvasScene::removeSceneItem(BaseItem *item)
{
	removeItem(item);
	item->setUpdateSceneOnRefresh(false);
}

void CanvasScene::onUserJoined(int id)
{
	UserMarkerItem *um = m_userMarkers.value(id);
	if(um) {
		m_userMarkers.remove(id);
		delete um;
	}
}

void CanvasScene::onCursorMoved(
	unsigned int flags, int userId, int layerId, int x, int y)
{
	bool valid = flags & DP_USER_CURSOR_FLAG_VALID;

	// User cursor motion is used to update the laser trail, if one exists
	if(valid && m_canvasVisible) {
		LaserTrailItem *item = m_activeLaserTrails.value(userId);
		if(item) {
			item->addPoint(QPointF(x, y));
		}
	}

	bool shouldShow =
		m_canvasModel && m_canvasVisible && m_showUserMarkers &&
		(m_showOwnUserMarker || userId != m_canvasModel->localUserId());
	if(shouldShow) {
		UserMarkerItem *item = m_userMarkers[userId];
		bool penUp = flags & DP_USER_CURSOR_FLAG_PEN_UP;
		bool penDown = flags & DP_USER_CURSOR_FLAG_PEN_DOWN;
		if(!item && valid) {
			const auto user = m_canvasModel->userlist()->getUserById(userId);
			item = new UserMarkerItem(
				userId, m_userMarkerPersistence, m_canvasGroup);
			item->setUpdateSceneOnRefresh(true);
			item->setText(
				user.name.isEmpty() ? QStringLiteral("#%1").arg(int(userId))
									: user.name);
			item->setShowText(m_showUserNames);
			item->setShowSubtext(m_showUserLayers);
			item->setAvatar(user.avatar);
			item->setShowAvatar(m_showUserAvatars);
			item->setEvadeCursor(m_evadeUserCursors);
			item->setCursorPosValid(m_cursorOnCanvas && !m_showOwnUserMarker);
			item->setCursorPos(m_cursorPos);
			item->setTargetPos(x, y, true);
			m_userMarkers[userId] = item;
		}

		if(item) {
			if(valid) {
				if(m_showUserLayers) {
					item->setSubtext(
						m_canvasModel->layerlist()
							->layerIndex(layerId)
							.data(canvas::LayerListModel::TitleRole)
							.toString());
				}
				item->setTargetPos(
					x, y,
					item->clearPenUp() || (penUp && penDown) ||
						!(flags & DP_USER_CURSOR_FLAG_INTERPOLATE));
				item->fadein();
			}

			if(penUp && !penDown) {
				item->setPenUp(true);
			}
		}
	}
}

void CanvasScene::onLaserTrail(int userId, int persistence, const QColor &color)
{
	if(persistence == 0) {
		m_activeLaserTrails.remove(userId);
	} else if(m_showLaserTrails && m_canvasVisible) {
		LaserTrailItem *item =
			new LaserTrailItem(userId, persistence, color, m_canvasGroup);
		item->setUpdateSceneOnRefresh(true);
		m_activeLaserTrails[userId] = item;
	}
}

void CanvasScene::setSelection(
	const QSharedPointer<canvas::SelectionMask> &mask)
{
	if(mask) {
		if(!m_selection) {
			m_selection = new SelectionItem(
				m_selectionIgnored, m_showSelectionMask, m_zoom, m_canvasGroup);
			m_selection->setUpdateSceneOnRefresh(true);
		}
		m_selection->setModel(mask);
		m_selection->setTransparentDelay(0.0);
	} else {
		delete m_selection;
		m_selection = nullptr;
	}
}

void CanvasScene::onTransformChanged()
{
	bool hadTransform = m_transform;
	canvas::TransformModel *transform =
		m_canvasModel ? m_canvasModel->transform() : nullptr;
	bool active = transform && transform->isActive();
	if(active) {
		const TransformQuad &quad = transform->dstQuad();
		bool valid = transform->isDstQuadValid();
		if(m_transform) {
			m_transform->setQuad(quad, valid);
		} else {
			m_transform = new TransformItem(quad, valid, m_zoom, m_canvasGroup);
			m_transform->setUpdateSceneOnRefresh(true);
		}
		// Accurate previews happen in the paint engine, fast ones in the item.
		if(transform->isPreviewAccurate()) {
			m_transform->setPreviewImage(QImage());
		} else {
			m_transform->setPreviewImage(transform->floatingImage());
		}
	} else if(m_transform) {
		delete m_transform;
		m_transform = nullptr;
	}

	if(m_selection) {
		bool haveTransform = m_transform;
		m_selection->updateVisibility(!haveTransform);
		// Bit of a hack to mitigate the selection flickering back to the old
		// location after a transform and then stumbling into the new one by
		// delaying its display slightly if it was just applied.
		bool shouldDelaySelection = hadTransform && !haveTransform &&
									transform && transform->isJustApplied();
		m_selection->setTransparentDelay(shouldDelaySelection ? 0.5 : 0.0);
	}
}

void CanvasScene::onAnnotationsChanged(const drawdance::AnnotationList &al)
{
	QHash<int, AnnotationItem *> annotationItems;
	for(QGraphicsItem *item : m_canvasGroup->childItems()) {
		AnnotationItem *ai = qgraphicsitem_cast<AnnotationItem *>(item);
		if(ai) {
			annotationItems.insert(ai->id(), ai);
		}
	}

	int count = al.count();
	for(int i = 0; i < count; ++i) {
		drawdance::Annotation a = al.at(i);
		int id = a.id();
		QHash<int, AnnotationItem *>::iterator it = annotationItems.find(id);
		AnnotationItem *ai;
		if(it == annotationItems.end()) {
			ai = new AnnotationItem(id, m_zoom, m_canvasGroup);
			ai->setUpdateSceneOnRefresh(true);
			ai->setShowBorder(m_showAnnotationBorders);
			ai->setVisible(m_showAnnotations);
		} else {
			ai = it.value();
			annotationItems.erase(it);
		}
		ai->setText(a.text());
		bool resized = ai->setGeometry(a.bounds());
		ai->setColor(a.backgroundColor());
		ai->setProtect(a.protect());
		ai->setAlias(a.alias());
		ai->setRasterize(a.rasterize());
		ai->setValign(a.valign());
		if(resized) {
			emit annotationResized(id);
		}
	}

	for(AnnotationItem *ai : annotationItems) {
		emit annotationDeleted(ai->id());
		delete ai;
	}
}

void CanvasScene::onPreviewAnnotation(int annotationId, const QRect &shape)
{
	AnnotationItem *ai = getAnnotationItem(annotationId);
	if(!ai) {
		ai = new AnnotationItem(annotationId, m_zoom, m_canvasGroup);
		ai->setUpdateSceneOnRefresh(true);
		ai->setShowBorder(m_showAnnotationBorders);
		ai->setVisible(m_showAnnotations);
	}
	if(ai->setGeometry(shape)) {
		emit annotationResized(annotationId);
	}
}

void CanvasScene::advanceAnimations()
{
	qreal dt = qreal(m_animationElapsedTimer.elapsed()) / 1000.0;

	for(QGraphicsItem *item : m_canvasGroup->childItems()) {
		if(LaserTrailItem *lt = qgraphicsitem_cast<LaserTrailItem *>(item)) {
			if(!lt->animationStep(dt)) {
				int owner = lt->owner();
				if(m_activeLaserTrails.value(owner)) {
					m_activeLaserTrails.remove(owner);
				}
				delete item;
			}

		} else if(
			UserMarkerItem *um = qgraphicsitem_cast<UserMarkerItem *>(item)) {
			um->animationStep(dt);
		}
	}

	if(m_selection) {
		m_selection->animationStep(dt);
	}

	m_hud->advanceAnimations(dt);

	m_animationElapsedTimer.restart();
}

}
