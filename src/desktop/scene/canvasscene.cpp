// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/canvasscene.h"
#include "desktop/scene/annotationitem.h"
#include "desktop/scene/canvasitem.h"
#include "desktop/scene/catchupitem.h"
#include "desktop/scene/lasertrailitem.h"
#include "desktop/scene/maskpreviewitem.h"
#include "desktop/scene/noticeitem.h"
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
#include "libclient/drawdance/annotationlist.h"
#include <QApplication>
#include <QDebug>
#include <QGraphicsItemGroup>
#include <QTimer>
#include <utility>
#ifdef HAVE_EMULATED_BITMAP_CURSOR
#	include "desktop/scene/cursoritem.h"
#endif

namespace drawingboard {

CanvasScene::CanvasScene(QObject *parent)
	: QGraphicsScene(parent)
	, m_model(nullptr)
	, m_maskPreview(nullptr)
	, m_pathPreview(nullptr)
	, m_selection(nullptr)
	, m_transform(nullptr)
	, m_transformNotice(nullptr)
	, m_lockNotice(nullptr)
	, m_toolNotice(nullptr)
	, m_catchup(nullptr)
	, m_streamResetNotice(nullptr)
	, m_showAnnotationBorders(false)
	, m_showAnnotations(true)
	, m_showUserMarkers(true)
	, m_showUserNames(true)
	, m_showUserLayers(true)
	, m_showUserAvatars(true)
	, m_evadeUserCursors(true)
	, m_showLaserTrails(true)
	, m_showOwnUserMarker(false)
	, m_cursorOnCanvas(false)
	, m_selectionIgnored(false)
	, m_showSelectionMask(false)
	, m_userMarkerPersistence(1000)
{
	setItemIndexMethod(NoIndex);
	setSceneRect(QRectF{0.0, 0.0, 1.0, 1.0});

	m_group = new QGraphicsItemGroup;
	addItem(m_group);

	m_canvasItem = new CanvasItem{m_group};

	m_outlineItem = new OutlineItem;
	addItem(m_outlineItem);
#ifdef HAVE_EMULATED_BITMAP_CURSOR
	m_cursorItem = new CursorItem;
	addItem(m_cursorItem);
#endif

	// Timer for on-canvas animations (user pointer fadeout, laser trail
	// flickering and such) Our animation needs are very simple, so we use this
	// instead of QGraphicsScene own animation system.
	auto animationTimer = new QTimer(this);
	connect(
		animationTimer, &QTimer::timeout, this,
		&CanvasScene::advanceAnimations);
	animationTimer->setInterval(20);
	animationTimer->start(20);
}

/**
 * This prepares the canvas for new drawing commands.
 * @param myid the context id of the local user
 */
void CanvasScene::initCanvas(canvas::CanvasModel *model)
{
	m_model = model;
	m_canvasItem->setPaintEngine(m_model->paintEngine());

	connect(
		m_model->paintEngine(), &canvas::PaintEngine::resized, this,
		&CanvasScene::handleCanvasResize, Qt::QueuedConnection);
	connect(
		m_model->paintEngine(), &canvas::PaintEngine::annotationsChanged, this,
		&CanvasScene::annotationsChanged);
	connect(
		m_model->paintEngine(), &canvas::PaintEngine::cursorMoved, this,
		&CanvasScene::userCursorMoved);

	connect(
		m_model, &canvas::CanvasModel::previewAnnotationRequested, this,
		&CanvasScene::previewAnnotation);
	connect(
		m_model, &canvas::CanvasModel::laserTrail, this,
		&CanvasScene::laserTrail);
	connect(
		m_model, &canvas::CanvasModel::userJoined, this,
		&CanvasScene::onUserJoined);
	connect(
		m_model->selection(), &canvas::SelectionModel::selectionChanged, this,
		&CanvasScene::setSelection);
	connect(
		m_model->transform(), &canvas::TransformModel::transformChanged, this,
		&CanvasScene::onTransformChanged);

	for(QGraphicsItem *item : m_group->childItems()) {
		if(item->type() == AnnotationItem::Type ||
		   item->type() == UserMarkerItem::Type ||
		   item->type() == LaserTrailItem::Type) {
			delete item;
		}
	}
	m_usermarkers.clear();
	m_activeLaserTrail.clear();

	canvas::SelectionModel *selection = m_model->selection();
	setSelection(selection->isValid(), selection->bounds(), selection->mask());
	onTransformChanged();

	QList<QRectF> regions;
	regions.append(sceneRect());
	emit changed(regions);
}

QTransform CanvasScene::canvasTransform() const
{
	return m_group->transform();
}

void CanvasScene::setCanvasTransform(const QTransform &matrix)
{
	m_group->setTransform(matrix);
}

void CanvasScene::setZoom(qreal zoom)
{
	if(zoom != m_zoom) {
		m_zoom = zoom;
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

void CanvasScene::setCanvasPixelGrid(bool pixelGrid)
{
	m_canvasItem->setPixelGrid(pixelGrid);
}

QRectF CanvasScene::canvasBounds() const
{
	return m_group->transform()
		.map(m_group->childrenBoundingRect())
		.boundingRect();
}

void CanvasScene::setSceneBounds(const QRectF &sceneBounds)
{
	m_sceneBounds = sceneBounds;
	if(m_transformNotice) {
		setTransformNoticePosition();
	}
	if(m_lockNotice) {
		setLockNoticePosition();
	}
	if(m_toolNotice) {
		setToolNoticePosition(false);
	}
	if(m_catchup) {
		setCatchupPosition();
	}
	if(m_streamResetNotice) {
		setStreamResetNoticePosition();
	}
	for(ToggleItem *ti : m_toggleItems) {
		ti->updateSceneBounds(sceneBounds);
	}
}

void CanvasScene::setNotificationBarHeight(int height)
{
	qreal topOffset = height;
	if(m_topOffset != topOffset) {
		m_topOffset = topOffset;
		if(m_transformNotice) {
			setTransformNoticePosition();
		}
		if(m_lockNotice) {
			setLockNoticePosition();
		}
	}
}

void CanvasScene::showTransformNotice(const QString &text)
{
	if(m_transformNotice) {
		m_transformNotice->setText(text);
	} else {
		m_transformNotice = new NoticeItem{text};
		addItem(m_transformNotice);
	}
	setTransformNoticePosition();
	m_transformNotice->setPersist(NOTICE_PERSIST);
}

void CanvasScene::showLockNotice(const QString &text)
{
	if(m_lockNotice) {
		m_lockNotice->setText(text);
	} else {
		m_lockNotice = new NoticeItem{text};
		addItem(m_lockNotice);
	}
	setLockNoticePosition();
}

void CanvasScene::hideLockNotice()
{
	delete m_lockNotice;
	m_lockNotice = nullptr;
}

void CanvasScene::setToolNotice(const QString &text)
{
	if(text.isEmpty()) {
		if(m_toolNotice) {
			delete m_toolNotice;
			m_toolNotice = nullptr;
		}
	} else {
		if(m_toolNotice) {
			if(m_toolNotice->setText(text)) {
				setToolNoticePosition(false);
			}
		} else {
			m_toolNotice = new NoticeItem(text);
			m_toolNotice->setOpacity(0.7);
			m_toolNotice->setZValue(BaseItem::Z_TOOL_NOTICE);
			addItem(m_toolNotice);
			setToolNoticePosition(true);
		}
	}
}

ToggleItem::Action
CanvasScene::checkHover(const QPointF &scenePos, bool *outWasHovering)
{
	ToggleItem::Action action = ToggleItem::Action::None;
	bool wasHovering = false;
	for(ToggleItem *ti : m_toggleItems) {
		if(ti->checkHover(scenePos, wasHovering)) {
			action = ti->action();
		}
	}
	if(outWasHovering) {
		*outWasHovering = wasHovering;
	}
	return action;
}

void CanvasScene::removeHover()
{
	for(ToggleItem *ti : m_toggleItems) {
		ti->removeHover();
	}
}

void CanvasScene::showCanvas()
{
	m_canvasItem->setVisible(true);
}

void CanvasScene::hideCanvas()
{
	m_canvasItem->setVisible(false);
}

void CanvasScene::canvasViewportChanged(const QPolygonF &viewport)
{
	m_canvasItem->setViewportBounds(viewport.boundingRect());
}

void CanvasScene::setCatchupProgress(int percent)
{
	if(!m_catchup) {
		m_catchup = new CatchupItem(tr("Restoring canvas…"));
		addItem(m_catchup);
	}
	m_catchup->setCatchupProgress(percent);
	setCatchupPosition();
}

void CanvasScene::setStreamResetProgress(int percent)
{
	if(percent > 100) {
		if(m_streamResetNotice) {
			m_streamResetNotice->setText(
				view::CanvasScene::getStreamResetProgressText(percent));
			if(m_streamResetNotice->persist() < 0.0) {
				m_streamResetNotice->setPersist(NOTICE_PERSIST);
			}
		}
	} else {
		if(m_streamResetNotice) {
			m_streamResetNotice->setText(
				view::CanvasScene::getStreamResetProgressText(percent));
			m_streamResetNotice->setPersist(-1.0);
		} else {
			m_streamResetNotice = new NoticeItem(
				view::CanvasScene::getStreamResetProgressText(percent));
			addItem(m_streamResetNotice);
		}
		setStreamResetNoticePosition();
	}
}

void CanvasScene::onUserJoined(int id, const QString &name)
{
	Q_UNUSED(name);
	UserMarkerItem *um = m_usermarkers.value(id);
	if(um) {
		m_usermarkers.remove(id);
		delete um;
	}
}

void CanvasScene::showAnnotations(bool show)
{
	if(m_showAnnotations != show) {
		m_showAnnotations = show;
		for(QGraphicsItem *item : m_group->childItems()) {
			if(item->type() == AnnotationItem::Type) {
				item->setVisible(show);
			}
		}
	}
}

void CanvasScene::showAnnotationBorders(bool showBorders)
{
	if(m_showAnnotationBorders != showBorders) {
		m_showAnnotationBorders = showBorders;
		for(QGraphicsItem *item : m_group->childItems()) {
			AnnotationItem *ai = qgraphicsitem_cast<AnnotationItem *>(item);
			if(ai) {
				ai->setShowBorder(showBorders);
			}
		}
	}
}

void CanvasScene::handleCanvasResize(
	const QSize newSize, const QPoint &offset, const QSize &oldSize)
{
	Q_UNUSED(newSize);
	emit canvasResized(offset.x(), offset.y(), oldSize);
}

AnnotationItem *CanvasScene::getAnnotationItem(int id)
{
	for(QGraphicsItem *i : m_group->childItems()) {
		AnnotationItem *item = qgraphicsitem_cast<AnnotationItem *>(i);
		if(item && item->id() == id) {
			return item;
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

void CanvasScene::setShowSelectionMask(bool showSelectionMask)
{
	m_showSelectionMask = showSelectionMask;
	if(m_selection) {
		m_selection->setShowMask(showSelectionMask);
	}
}

void CanvasScene::setTransformToolState(int mode, int handle, bool dragging)
{
	if(m_transform) {
		m_transform->setToolState(mode, handle, dragging);
	}
}

void CanvasScene::setActiveAnnotation(int id)
{
	for(QGraphicsItem *i : m_group->childItems()) {
		AnnotationItem *item = qgraphicsitem_cast<AnnotationItem *>(i);
		if(item) {
			item->setHighlight(item->id() == id);
		}
	}
}

void CanvasScene::annotationsChanged(const drawdance::AnnotationList &al)
{
	QHash<int, AnnotationItem *> annotationItems;
	for(QGraphicsItem *item : m_group->childItems()) {
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
			ai = new AnnotationItem(id, m_group);
			ai->setShowBorder(m_showAnnotationBorders);
			ai->setVisible(m_showAnnotations);
		} else {
			ai = it.value();
			annotationItems.erase(it);
		}
		ai->setText(a.text());
		ai->setGeometry(a.bounds());
		ai->setColor(a.backgroundColor());
		ai->setProtect(a.protect());
		ai->setValign(a.valign());
	}

	for(AnnotationItem *ai : annotationItems) {
		emit annotationDeleted(ai->id());
		delete ai;
	}
}

void CanvasScene::previewAnnotation(int id, const QRect &shape)
{
	AnnotationItem *ai = getAnnotationItem(id);
	if(!ai) {
		ai = new AnnotationItem(id, m_group);
		ai->setShowBorder(m_showAnnotationBorders);
		ai->setVisible(m_showAnnotations);
	}

	ai->setGeometry(shape);
}

void CanvasScene::setSelection(
	bool valid, const QRect &bounds, const QImage &mask)
{
	if(valid) {
		if(!m_selection) {
			m_selection = new SelectionItem(
				m_selectionIgnored, m_showSelectionMask, m_zoom, m_group);
			m_selection->setUpdateSceneOnRefresh(true);
		}
		m_selection->setModel(bounds, mask);
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
		m_model ? m_model->transform() : nullptr;
	bool active = transform && transform->isActive();
	if(active) {
		const TransformQuad &quad = transform->dstQuad();
		bool valid = transform->isDstQuadValid();
		if(m_transform) {
			m_transform->setQuad(quad, valid);
		} else {
			m_transform = new TransformItem(quad, valid, m_zoom, m_group);
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

/**
 * @brief Advance canvas animations
 *
 * Note. We don't use the scene's built-in animation features since we care
 * about just a few specific animations.
 */
void CanvasScene::advanceAnimations()
{
	const qreal STEP = 0.02; // time delta in seconds

	for(QGraphicsItem *item : m_group->childItems()) {
		if(LaserTrailItem *lt = qgraphicsitem_cast<LaserTrailItem *>(item)) {
			if(!lt->animationStep(STEP)) {
				if(m_activeLaserTrail.value(lt->owner()) == lt) {
					m_activeLaserTrail.remove(lt->owner());
				}
				delete item;
			}

		} else if(
			UserMarkerItem *um = qgraphicsitem_cast<UserMarkerItem *>(item)) {
			um->animationStep(STEP);
		}
	}

	if(m_selection) {
		m_selection->animationStep(STEP);
	}

	if(m_transformNotice && !m_transformNotice->animationStep(STEP)) {
		delete m_transformNotice;
		m_transformNotice = nullptr;
	}

	if(m_catchup && !m_catchup->animationStep(STEP)) {
		delete m_catchup;
		m_catchup = nullptr;
	}

	if(m_streamResetNotice && !m_streamResetNotice->animationStep(STEP)) {
		delete m_streamResetNotice;
		m_streamResetNotice = nullptr;
	}
}

void CanvasScene::laserTrail(int userId, int persistence, const QColor &color)
{
	if(persistence == 0) {
		m_activeLaserTrail.remove(userId);
	} else if(m_showLaserTrails) {
		LaserTrailItem *item =
			new LaserTrailItem(userId, persistence, color, m_group);
		m_activeLaserTrail[userId] = item;
	}
}

void CanvasScene::userCursorMoved(
	unsigned int flags, int userId, int layerId, int x, int y)
{
	bool valid = flags & DP_USER_CURSOR_FLAG_VALID;

	// User cursor motion is used to update the laser trail, if one exists
	if(valid && m_activeLaserTrail.contains(userId)) {
		LaserTrailItem *laser = m_activeLaserTrail[userId];
		laser->addPoint(QPointF(x, y));
	}

	if(!m_showUserMarkers || !m_canvasItem->isVisible()) {
		return;
	}

	if(!m_showOwnUserMarker && userId == m_model->localUserId()) {
		return;
	}

	UserMarkerItem *item = m_usermarkers[userId];
	bool penUp = flags & DP_USER_CURSOR_FLAG_PEN_UP;
	bool penDown = flags & DP_USER_CURSOR_FLAG_PEN_DOWN;
	if(!item && valid) {
		const auto user = m_model->userlist()->getUserById(userId);
		item = new UserMarkerItem(userId, m_userMarkerPersistence, m_group);
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
		m_usermarkers[userId] = item;
	}

	if(item) {
		if(valid) {
			if(m_showUserLayers) {
				item->setSubtext(m_model->layerlist()
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

void CanvasScene::showUserMarkers(bool show)
{
	if(m_showUserMarkers != show) {
		m_showUserMarkers = show;
		if(!show) {
			for(UserMarkerItem *item : std::as_const(m_usermarkers)) {
				item->hide();
			}
		}
	}
}

void CanvasScene::showUserNames(bool show)
{
	if(m_showUserNames != show) {
		m_showUserNames = show;
		for(UserMarkerItem *item : std::as_const(m_usermarkers)) {
			item->setShowText(show);
		}
	}
}

void CanvasScene::showUserLayers(bool show)
{
	if(m_showUserLayers != show) {
		m_showUserLayers = show;
		for(UserMarkerItem *item : std::as_const(m_usermarkers)) {
			item->setShowSubtext(show);
		}
	}
}

void CanvasScene::showUserAvatars(bool show)
{
	if(m_showUserAvatars != show) {
		m_showUserAvatars = show;
		for(UserMarkerItem *item : std::as_const(m_usermarkers)) {
			item->setShowAvatar(show);
		}
	}
}

void CanvasScene::setEvadeUserCursors(bool evadeUserCursors)
{
	if(m_evadeUserCursors != evadeUserCursors) {
		m_evadeUserCursors = evadeUserCursors;
		for(UserMarkerItem *item : std::as_const(m_usermarkers)) {
			item->setEvadeCursor(evadeUserCursors);
		}
	}
}

void CanvasScene::showLaserTrails(bool show)
{
	m_showLaserTrails = show;
	if(!show) {
		m_activeLaserTrail.clear();
		for(QGraphicsItem *item : m_group->childItems()) {
			if(item->type() == LaserTrailItem::Type) {
				delete item;
			}
		}
	}
}

void CanvasScene::showToggleItems(bool show)
{
	if(show && m_toggleItems.isEmpty()) {
		m_toggleItems = {
			new ToggleItem{
				ToggleItem::Action::Left, Qt::AlignLeft, 1.0 / 3.0,
				QIcon::fromTheme("draw-brush")},
			new ToggleItem{
				ToggleItem::Action::Top, Qt::AlignLeft, 2.0 / 3.0,
				QIcon::fromTheme("keyframe")},
			new ToggleItem{
				ToggleItem::Action::Right, Qt::AlignRight, 1.0 / 3.0,
				QIcon::fromTheme("layer-visible-on")},
			new ToggleItem{
				ToggleItem::Action::Bottom, Qt::AlignRight, 2.0 / 3.0,
				QIcon::fromTheme("edit-comment")},
		};
		for(ToggleItem *ti : m_toggleItems) {
			addItem(ti);
			ti->updateSceneBounds(m_sceneBounds);
		}
	} else if(!show && !m_toggleItems.isEmpty()) {
		for(ToggleItem *ti : m_toggleItems) {
			removeItem(ti);
		}
		m_toggleItems.clear();
	}
}

void CanvasScene::setTransformNoticePosition()
{
	m_transformNotice->setPos(
		m_sceneBounds.topLeft() +
		QPointF(NOTICE_OFFSET, NOTICE_OFFSET + m_topOffset));
}

void CanvasScene::setLockNoticePosition()
{
	m_lockNotice->setPos(
		m_sceneBounds.topRight() +
		QPointF(
			-m_lockNotice->boundingRect().width() - NOTICE_OFFSET,
			NOTICE_OFFSET + m_topOffset));
}

void CanvasScene::setToolNoticePosition(bool initial)
{
	QSizeF size = m_toolNotice->boundingRect().size();
	QPointF pos;
	if(m_cursorOnCanvas) {
		pos = m_cursorPos + QPointF(TOOL_NOTICE_OFFSET, TOOL_NOTICE_OFFSET);
	} else if(initial) {
		pos = m_sceneBounds.center() -
			  QPointF(size.width() / 2.0, size.height() / 2.0);
	} else {
		pos = m_toolNotice->pos();
	}
	m_toolNotice->updatePosition(
		utils::moveRectToFitF(QRectF(pos, size), m_sceneBounds).topLeft());
}

void CanvasScene::setCatchupPosition()
{
	QRectF catchupBounds = m_catchup->boundingRect();
	m_catchup->setPos(
		m_sceneBounds.bottomRight() -
		QPointF(
			catchupBounds.width() + NOTICE_OFFSET,
			catchupBounds.height() + NOTICE_OFFSET));
}

void CanvasScene::setStreamResetNoticePosition()
{
	qreal catchupOffset =
		m_catchup ? m_catchup->boundingRect().height() + NOTICE_OFFSET : 0.0;
	QRectF streamResetNoticeBounds = m_streamResetNotice->boundingRect();
	m_streamResetNotice->setPos(
		m_sceneBounds.bottomRight() -
		QPointF(
			streamResetNoticeBounds.width() + NOTICE_OFFSET,
			streamResetNoticeBounds.height() + NOTICE_OFFSET + catchupOffset));
}

void CanvasScene::setShowOwnUserMarker(bool showOwnUserMarker)
{
	if(showOwnUserMarker != m_showOwnUserMarker) {
		m_showOwnUserMarker = showOwnUserMarker;
		for(UserMarkerItem *item : std::as_const(m_usermarkers)) {
			item->setCursorPosValid(m_cursorOnCanvas && !showOwnUserMarker);
		}
	}
}

void CanvasScene::setUserMarkerPersistence(int userMarkerPersistence)
{
	if(userMarkerPersistence != m_userMarkerPersistence) {
		m_userMarkerPersistence = userMarkerPersistence;
		for(UserMarkerItem *item : std::as_const(m_usermarkers)) {
			item->setPersistence(userMarkerPersistence);
		}
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
		m_maskPreview = new MaskPreviewItem(mask, m_group);
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
		m_pathPreview = new PathPreviewItem(path, m_zoom, m_group);
	}
}

void CanvasScene::setOutline(qreal size, qreal width)
{
	m_outlineItem->setOutline(size, width);
}

void CanvasScene::setOutlineTransform(const QPointF &pos, qreal angle)
{
	m_outlineItem->setPos(pos);
	m_outlineItem->setRotation(angle);
}

void CanvasScene::setOutlineSquare(bool square)
{
	m_outlineItem->setSquare(square);
}

void CanvasScene::setOutlineWidth(qreal width)
{
	m_outlineItem->setOutlineWidth(width);
}

void CanvasScene::setOutlineVisibleInMode(bool visibleInMode)
{
	m_outlineItem->setVisibleInMode(visibleInMode);
}

void CanvasScene::setCursorOnCanvas(bool onCanvas)
{
	if(onCanvas != m_cursorOnCanvas) {
		m_cursorOnCanvas = onCanvas;
		m_outlineItem->setOnCanvas(onCanvas);
#ifdef HAVE_EMULATED_BITMAP_CURSOR
		m_cursorItem->setOnCanvas(onCanvas);
#endif
		for(UserMarkerItem *item : std::as_const(m_usermarkers)) {
			item->setCursorPosValid(onCanvas && !m_showOwnUserMarker);
		}
	}
}

void CanvasScene::setCursorPos(const QPointF &pos)
{
	if(pos != m_cursorPos) {
		m_cursorPos = pos;
#ifdef HAVE_EMULATED_BITMAP_CURSOR
		m_cursorItem->setPos(pos);
#endif
		for(UserMarkerItem *item : std::as_const(m_usermarkers)) {
			item->setCursorPos(pos);
		}
		if(m_toolNotice) {
			setToolNoticePosition(false);
		}
	}
}

#ifdef HAVE_EMULATED_BITMAP_CURSOR
void CanvasScene::setCursor(const QCursor &cursor)
{
	m_cursorItem->setCursor(cursor);
}
#endif

}
