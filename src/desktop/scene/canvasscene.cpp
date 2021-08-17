/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2019 Calle Laakkonen

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
#include <QDebug>
#include <QTimer>
#include <QApplication>

#include "scene/canvasscene.h"
#include "scene/canvasitem.h"
#include "scene/selectionitem.h"
#include "scene/annotationitem.h"
#include "scene/usermarkeritem.h"
#include "scene/lasertrailitem.h"

#include "canvas/canvasmodel.h"
#include "canvas/paintengine.h"
#include "canvas/usercursormodel.h"
#include "canvas/lasertrailmodel.h"
#include "core/layerstack.h"
#include "core/layer.h"

#include "../rustpile/rustpile.h"

namespace drawingboard {

CanvasScene::CanvasScene(QObject *parent)
	: QGraphicsScene(parent), m_model(nullptr),
	  m_selection(nullptr),
	  m_showAnnotationBorders(false), m_showAnnotations(true),
	  m_showUserMarkers(true), m_showUserNames(true), m_showUserLayers(true), m_showUserAvatars(true),
	  m_showLaserTrails(true)
{
	setItemIndexMethod(NoIndex);

	m_canvasItem = new CanvasItem;
	addItem(m_canvasItem);

	// Timer for on-canvas animations (user pointer fadeout, laser trail flickering and such)
	auto animationTimer = new QTimer(this);
	connect(animationTimer, &QTimer::timeout, this, &CanvasScene::advanceUsermarkerAnimation);
	animationTimer->setInterval(200);
	animationTimer->start(200);
}

/**
 * This prepares the canvas for new drawing commands.
 * @param myid the context id of the local user
 */
void CanvasScene::initCanvas(canvas::CanvasModel *model)
{
	if(model == m_model) {
		qWarning("initCanvas already called on this model");
		return;
	}

	onSelectionChanged(nullptr);

	m_model = model;

	connect(m_model->paintEngine(), &canvas::PaintEngine::resized, this, &CanvasScene::handleCanvasResize);
	connect(m_model->paintEngine(), &canvas::PaintEngine::annotationsChanged, this, &CanvasScene::annotationsChanged);
	connect(m_model, &canvas::CanvasModel::previewAnnotationRequested, this, &CanvasScene::previewAnnotation);

	m_canvasItem->setPaintEngine(m_model->paintEngine());

	canvas::UserCursorModel *cursors = m_model->userCursors();
	connect(cursors, &canvas::UserCursorModel::rowsInserted, this, &CanvasScene::userCursorAdded);
	connect(cursors, &canvas::UserCursorModel::dataChanged, this, &CanvasScene::userCursorChanged);
	connect(cursors, &canvas::UserCursorModel::rowsAboutToBeRemoved, this, &CanvasScene::userCursorRemoved);

	canvas::LaserTrailModel *lasers = m_model->laserTrails();
	connect(lasers, &canvas::LaserTrailModel::rowsInserted, this, &CanvasScene::laserAdded);
	connect(lasers, &canvas::LaserTrailModel::dataChanged, this, &CanvasScene::laserChanged);
	connect(lasers, &canvas::LaserTrailModel::rowsAboutToBeRemoved, this, &CanvasScene::laserRemoved);

	connect(m_model, &canvas::CanvasModel::selectionChanged, this, &CanvasScene::onSelectionChanged);

	for(QGraphicsItem *item : items()) {
		if(item->type() == AnnotationItem::Type || item->type() == UserMarkerItem::Type) {
			delete item;
		}
	}
	m_usermarkers.clear();
	
	QList<QRectF> regions;
	regions.append(sceneRect());
	emit changed(regions);
}

void CanvasScene::showCanvas()
{
	if(m_canvasItem)
		m_canvasItem->setVisible(true);
}

void CanvasScene::hideCanvas()
{
	if(m_canvasItem)
		m_canvasItem->setVisible(false);
}

void CanvasScene::onSelectionChanged(canvas::Selection *selection)
{
	if(m_selection) {
		removeItem(m_selection);
		delete m_selection;
		m_selection = nullptr;
	}
	if(selection) {
		m_selection = new SelectionItem(selection);
		addItem(m_selection);
	}
}

void CanvasScene::showAnnotations(bool show)
{
	if(m_showAnnotations != show) {
		m_showAnnotations = show;
		for(QGraphicsItem *item : items()) {
			if(item->type() == AnnotationItem::Type)
				item->setVisible(show);
		}
	}
}

void CanvasScene::showAnnotationBorders(bool showBorders)
{
	if(m_showAnnotationBorders != showBorders) {
		m_showAnnotationBorders = showBorders;
		for(QGraphicsItem *item : items()) {
			auto *ai = qgraphicsitem_cast<AnnotationItem*>(item);
			if(ai)
				ai->setShowBorder(showBorders);
		}
	}
}

void CanvasScene::handleCanvasResize(int xoffset, int yoffset, const QSize &oldsize)
{
	if(!m_canvasItem)
		return;
	QRectF bounds = m_canvasItem->boundingRect();

	setSceneRect(bounds.adjusted(-MARGIN, -MARGIN, MARGIN, MARGIN));
	emit canvasResized(xoffset, yoffset, oldsize);
}

AnnotationItem *CanvasScene::getAnnotationItem(int id)
{
	for(QGraphicsItem *i : items()) {
		AnnotationItem *item = qgraphicsitem_cast<AnnotationItem*>(i);
		if(item && item->id() == id)
			return item;
	}
	return nullptr;
}

void CanvasScene::setActiveAnnotation(int id)
{
	for(QGraphicsItem *i : items()) {
		AnnotationItem *item = qgraphicsitem_cast<AnnotationItem*>(i);
		if(item)
			item->setHighlight(item->id() == id);
	}
}

void CanvasScene::annotationsChanged(rustpile::Annotations *annotations) {
	struct Context {
		QList<AnnotationItem*> items;
		QList<AnnotationItem*> newItems;
	} ctx;

	// Gather up all the existing annotation items
	for(QGraphicsItem *item : items()) {
		 auto ai = qgraphicsitem_cast<AnnotationItem*>(item);
		 if(ai)
			 ctx.items << ai;
	}

	// The update function
	auto update = [](void *ctx, rustpile::AnnotationID id, const char *text, uintptr_t textlen, rustpile::Rectangle rect, rustpile::Color background, bool protect, rustpile::VAlign valign) {
		auto context = reinterpret_cast<Context*>(ctx);
		AnnotationItem *ai = nullptr;
		for(int i=0;i<context->items.size();++i) {
			if(context->items.at(i)->id() == id) {
				ai = context->items.takeAt(i);
				break;
			}
		}

		if(!ai) {
			ai = new AnnotationItem(id);
			context->newItems << ai;
		}

		ai->setText(QString::fromUtf8(text, textlen));
		ai->setGeometry(QRect{rect.x, rect.y, rect.w, rect.h});
		ai->setColor(QColor::fromRgbF(background.r, background.g, background.b, background.a));
		ai->setProtect(protect);
		ai->setValign(int(valign));
	};

	// Update or create annotations
	rustpile::annotations_get_all(annotations, &ctx, update);
	rustpile::annotations_free(annotations);

	// Whatever remains in the list are annotations that were removed
	for(auto ai : qAsConst(ctx.items)) {
		delete ai;
	}

	// These were added
	for(auto ai : qAsConst(ctx.newItems)) {
		ai->setShowBorder(showAnnotationBorders());
		ai->setVisible(m_showAnnotations);
		addItem(ai);
	}
}

void CanvasScene::previewAnnotation(int id, const QRect &shape)
{
	auto ai = getAnnotationItem(id);
	if(!ai) {
		ai = new AnnotationItem(id);
		ai->setShowBorder(showAnnotationBorders());
		ai->setVisible(m_showAnnotations);
		addItem(ai);
	}

	ai->setGeometry(shape);
}

void CanvasScene::laserAdded(const QModelIndex&, int first, int last)
{
	if(!m_canvasItem)
		return;

	// Don't add new lasers when canvas is hidden
	// or when laser trails are disabled
	if(!m_canvasItem->isVisible() || !m_showLaserTrails)
		return;

	for(int i=first;i<=last;++i) {
		const QModelIndex l = m_model->laserTrails()->index(i);
		const int id = l.data(canvas::LaserTrailModel::InternalIdRole).toInt();
		if(m_lasertrails.contains(id)) {
			qWarning("Item for laser trail %d already exists!", id);
			continue;
		} else {
			LaserTrailItem *item = new LaserTrailItem;
			addItem(item);
			m_lasertrails[id] = item;
		}
		laserChanged(l, l, QVector<int>());
	}
}

void CanvasScene::laserRemoved(const QModelIndex&, int first, int last)
{
	for(int i=first;i<=last;++i) {
		const QModelIndex l = m_model->laserTrails()->index(i);
		const int id = l.data(canvas::LaserTrailModel::InternalIdRole).toInt();
		delete m_lasertrails.take(id);
	}

}

void CanvasScene::laserChanged(const QModelIndex &first, const QModelIndex &last, const QVector<int> &changed)
{
	if(!m_canvasItem || !m_canvasItem->isVisible() || !m_showLaserTrails)
		return;

	const int ifirst = first.row();
	const int ilast = last.row();
	for(int i=ifirst;i<=ilast;++i) {
		const QModelIndex l = m_model->laserTrails()->index(i);
		const int id = l.data(canvas::LaserTrailModel::InternalIdRole).toInt();
		if(!m_lasertrails.contains(id)) {
			qWarning("Laser trail %d changed, but not yet created!", id);
			continue;
		}
		LaserTrailItem *item = m_lasertrails[id];
		if(changed.isEmpty() || changed.contains(canvas::LaserTrailModel::ColorRole))
			item->setColor(l.data(canvas::LaserTrailModel::ColorRole).value<QColor>());

		if(changed.isEmpty() || changed.contains(canvas::LaserTrailModel::PointsRole))
			item->setPoints(l.data(canvas::LaserTrailModel::PointsRole).value<QVector<QPointF>>());

		if(changed.isEmpty() || changed.contains(canvas::LaserTrailModel::VisibleRole))
			item->setFadeVisible(l.data(canvas::LaserTrailModel::VisibleRole).toBool());
	}
}

/**
 * @brief Advance canvas animations
 *
 * Note. We don't use the scene's built-in animation features since we care about
 * just a few specific animations.
 */
void CanvasScene::advanceUsermarkerAnimation()
{
	const double STEP = 0.2; // time delta in seconds

	for(LaserTrailItem *lt : m_lasertrails)
		lt->animationStep(STEP);

	for(UserMarkerItem *um : m_usermarkers) {
		um->fadeoutStep(STEP);
	}

	if(m_selection)
		m_selection->marchingAnts();
}

void CanvasScene::userCursorAdded(const QModelIndex&, int first, int last)
{
	for(int i=first;i<=last;++i) {
		const QModelIndex um = m_model->userCursors()->index(i);
		const int id = um.data(canvas::UserCursorModel::IdRole).toInt();

		if(m_usermarkers.contains(id)) {
			qWarning("User marker item %d already exists!", id);

		} else {
			UserMarkerItem *item = new UserMarkerItem(id);
			item->setShowText(m_showUserNames);
			item->setShowSubtext(m_showUserLayers);
			item->setShowAvatar(m_showUserAvatars);
			item->hide();
			addItem(item);
			m_usermarkers[id] = item;
		}
		userCursorChanged(um, um, QVector<int>());
	}
}

void CanvasScene::userCursorRemoved(const QModelIndex&, int first, int last)
{
	for(int i=first;i<=last;++i) {
		const QModelIndex um = m_model->userCursors()->index(i);
		delete m_usermarkers.take(um.data(canvas::UserCursorModel::IdRole).toInt());
	}
}

void CanvasScene::userCursorChanged(const QModelIndex &first, const QModelIndex &last, const QVector<int> &changed)
{
	const int ifirst = first.row();
	const int ilast = last.row();
	for(int i=ifirst;i<=ilast;++i) {
		const QModelIndex um = m_model->userCursors()->index(i);
		int id = um.data(canvas::UserCursorModel::IdRole).toInt();
		if(!m_usermarkers.contains(id)) {
			qWarning("User marker %d changed, but not yet created!", id);
			continue;
		}
		UserMarkerItem *item = m_usermarkers[id];

		if(changed.isEmpty() || changed.contains(canvas::UserCursorModel::PositionRole))
			item->setPos(um.data(canvas::UserCursorModel::PositionRole).toPointF());

		if(changed.isEmpty() || changed.contains(Qt::DecorationRole))
			item->setAvatar(um.data(Qt::DecorationRole).value<QPixmap>());

		if(changed.isEmpty() || changed.contains(Qt::DisplayRole))
			item->setText(um.data(Qt::DisplayRole).toString());

		if(changed.isEmpty() || changed.contains(canvas::UserCursorModel::LayerRole))
			item->setSubtext(um.data(canvas::UserCursorModel::LayerRole).toString());

		if(changed.isEmpty() || changed.contains(canvas::UserCursorModel::ColorRole))
			item->setColor(um.data(canvas::UserCursorModel::ColorRole).value<QColor>());

		if(changed.isEmpty() || changed.contains(canvas::UserCursorModel::VisibleRole)) {
			if(m_showUserMarkers) {
				bool v = um.data(canvas::UserCursorModel::VisibleRole).toBool();
				if(v)
					item->fadein();
				else
					item->fadeout();
			}
		}
	}
}

void CanvasScene::showUserMarkers(bool show)
{
	if(m_showUserMarkers != show) {
		m_showUserMarkers = show;
		for(UserMarkerItem *item : m_usermarkers) {
			if(show) {
				if(m_model->userCursors()->indexForId(item->id()).data(canvas::UserCursorModel::VisibleRole).toBool())
					item->fadein();
			} else {
				item->hide();
			}
		}
	}
}

void CanvasScene::showUserNames(bool show)
{
	if(m_showUserNames != show) {
		m_showUserNames = show;
		for(UserMarkerItem *item : m_usermarkers)
			item->setShowText(show);
	}
}

void CanvasScene::showUserLayers(bool show)
{
	if(m_showUserLayers != show) {
		m_showUserLayers = show;
		for(UserMarkerItem *item : m_usermarkers)
			item->setShowSubtext(show);
	}
}

void CanvasScene::showUserAvatars(bool show)
{
	if(m_showUserAvatars != show) {
		m_showUserAvatars = show;
		for(UserMarkerItem *item : m_usermarkers)
			item->setShowAvatar(show);
	}
}

void CanvasScene::showLaserTrails(bool show)
{
	m_showLaserTrails = show;
	for(LaserTrailItem *i : m_lasertrails)
		i->hide();
}

}
