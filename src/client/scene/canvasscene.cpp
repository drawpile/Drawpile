/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2015 Calle Laakkonen

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
#include "canvas/statetracker.h"
#include "core/layerstack.h"
#include "core/layer.h"

namespace drawingboard {

CanvasScene::CanvasScene(QObject *parent)
	: QGraphicsScene(parent), _image(0), m_model(nullptr),
	  m_selection(nullptr),
	  _showAnnotationBorders(false), _showAnnotations(true), _showUserMarkers(true), _showUserLayers(true), _showLaserTrails(true), _thickLaserTrails(false)
{
	setItemIndexMethod(NoIndex);

	// Timer for on-canvas animations (user pointer fadeout, laser trail flickering and such)
	_animTickTimer = new QTimer(this);
	connect(_animTickTimer, &QTimer::timeout, this, &CanvasScene::advanceUsermarkerAnimation);
	_animTickTimer->setInterval(200);
	_animTickTimer->start(200);
}

CanvasScene::~CanvasScene()
{
	delete _image;
}

/**
 * This prepares the canvas for new drawing commands.
 * @param myid the context id of the local user
 */
void CanvasScene::initCanvas(canvas::CanvasModel *model)
{
	delete _image;

	m_model = model;

	_image = new CanvasItem(m_model->layerStack());

	connect(m_model->layerStack(), &paintcore::LayerStack::resized, this, &CanvasScene::handleCanvasResize);

	paintcore::AnnotationModel *anns = m_model->layerStack()->annotations();
	connect(anns, &paintcore::AnnotationModel::rowsInserted, this, &CanvasScene::annotationsAdded);
	connect(anns, &paintcore::AnnotationModel::dataChanged, this, &CanvasScene::annotationsChanged);
	connect(anns, &paintcore::AnnotationModel::rowsAboutToBeRemoved, this, &CanvasScene::annotationsRemoved);

	canvas::UserCursorModel *cursors = m_model->userCursors();
	connect(cursors, &canvas::UserCursorModel::rowsInserted, this, &CanvasScene::userCursorAdded);
	connect(cursors, &canvas::UserCursorModel::dataChanged, this, &CanvasScene::userCursorChanged);
	connect(cursors, &canvas::UserCursorModel::rowsAboutToBeRemoved, this, &CanvasScene::userCursorRemoved);

	canvas::LaserTrailModel *lasers = m_model->laserTrails();
	connect(lasers, &canvas::LaserTrailModel::rowsInserted, this, &CanvasScene::laserAdded);
	connect(lasers, &canvas::LaserTrailModel::dataChanged, this, &CanvasScene::laserChanged);
	connect(lasers, &canvas::LaserTrailModel::rowsAboutToBeRemoved, this, &CanvasScene::laserRemoved);

	connect(m_model, &canvas::CanvasModel::selectionChanged, this, &CanvasScene::onSelectionChanged);

	addItem(_image);

	// Clear out any old annotation items
	Q_FOREACH(QGraphicsItem *item, items()) {
		if(item->type() == AnnotationItem::Type) {
			delete item;
		}
	}

	Q_FOREACH(UserMarkerItem *i, m_usermarkers)
		delete i;
	m_usermarkers.clear();
	
	QList<QRectF> regions;
	regions.append(sceneRect());
	emit changed(regions);
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
	_showAnnotations = show;
	foreach(QGraphicsItem *item, items()) {
		if(item->type() == AnnotationItem::Type)
			item->setVisible(show);
	}
}

void CanvasScene::showAnnotationBorders(bool hl)
{
	_showAnnotationBorders = hl;
	foreach(QGraphicsItem *item, items()) {
		if(item->type() == AnnotationItem::Type)
			static_cast<AnnotationItem*>(item)->setShowBorder(hl);
	}
}

void CanvasScene::handleCanvasResize(int xoffset, int yoffset, const QSize &oldsize)
{
	QRectF bounds = _image->boundingRect();

	// Include some empty space around the canvas to make working
	// near the borders easier.
	const float wPadding = 300;
	const float hPadding = 300;

	setSceneRect(bounds.adjusted(-wPadding, -hPadding, wPadding, hPadding));
	emit canvasResized(xoffset, yoffset, oldsize);
}

AnnotationItem *CanvasScene::getAnnotationItem(int id)
{
	foreach(QGraphicsItem *i, items()) {
		if(i->type() == AnnotationItem::Type) {
			AnnotationItem *item = static_cast<AnnotationItem*>(i);
			if(item->id() == id)
				return item;
		}
	}
	return 0;
}

void CanvasScene::activeAnnotationChanged(int id)
{
	foreach(QGraphicsItem *i, items()) {
		if(i->type() == AnnotationItem::Type) {
			AnnotationItem *item = static_cast<AnnotationItem*>(i);
			item->setHighlight(item->id() == id);
		}
	}
}

void CanvasScene::annotationsAdded(const QModelIndex&, int first, int last)
{
	for(int i=first;i<=last;++i) {
		const QModelIndex a = m_model->layerStack()->annotations()->index(i);
		AnnotationItem *item = new AnnotationItem(a.data(paintcore::AnnotationModel::IdRole).toInt());
		item->setShowBorder(showAnnotationBorders());
		item->setVisible(_showAnnotations);
		addItem(item);
		annotationsChanged(a, a, QVector<int>());
	}
}

void CanvasScene::annotationsRemoved(const QModelIndex&, int first, int last)
{
	for(int i=first;i<=last;++i) {
		const QModelIndex a = m_model->layerStack()->annotations()->index(i);
		int id = a.data(paintcore::AnnotationModel::IdRole).toInt();
		AnnotationItem *item = getAnnotationItem(id);
		if(item)
			delete item;
	}

}

void CanvasScene::annotationsChanged(const QModelIndex &first, const QModelIndex &last, const QVector<int> &changed)
{
	const int ifirst = first.row();
	const int ilast = last.row();
	for(int i=ifirst;i<=ilast;++i) {
		const QModelIndex a = m_model->layerStack()->annotations()->index(i);
		AnnotationItem *item = getAnnotationItem(a.data(paintcore::AnnotationModel::IdRole).toInt());
		Q_ASSERT(item);
		if(changed.isEmpty() || changed.contains(Qt::DisplayRole))
			item->setText(a.data(Qt::DisplayRole).toString());

		if(changed.isEmpty() || changed.contains(paintcore::AnnotationModel::RectRole))
			item->setGeometry(a.data(paintcore::AnnotationModel::RectRole).toRect());

		if(changed.isEmpty() || changed.contains(paintcore::AnnotationModel::BgColorRole))
			item->setColor(a.data(paintcore::AnnotationModel::BgColorRole).value<QColor>());
	}
}

void CanvasScene::laserAdded(const QModelIndex&, int first, int last)
{
	for(int i=first;i<=last;++i) {
		const QModelIndex l = m_model->laserTrails()->index(i);
		LaserTrailItem *item = new LaserTrailItem(_thickLaserTrails);
		addItem(item);
		m_lasertrails[l.data(canvas::LaserTrailModel::InternalIdRole).toInt()] = item;
		laserChanged(l, l, QVector<int>());
	}
}

void CanvasScene::laserRemoved(const QModelIndex&, int first, int last)
{
	for(int i=first;i<=last;++i) {
		const QModelIndex l = m_model->laserTrails()->index(i);
		int id = l.data(canvas::LaserTrailModel::InternalIdRole).toInt();
		delete m_lasertrails.take(id);
	}

}

void CanvasScene::laserChanged(const QModelIndex &first, const QModelIndex &last, const QVector<int> &changed)
{
	const int ifirst = first.row();
	const int ilast = last.row();
	for(int i=ifirst;i<=ilast;++i) {
		const QModelIndex l = m_model->laserTrails()->index(i);
		int id = l.data(canvas::LaserTrailModel::InternalIdRole).toInt();
		if(!m_lasertrails.contains(id)) {
			qWarning("Laser trail %d changed, but not yet created!", id);
			return;
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
	const float STEP = 0.2; // time delta in seconds

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
		int id = um.data(canvas::UserCursorModel::IdRole).toInt();
		UserMarkerItem *item = new UserMarkerItem(id);
		item->setShowSubtext(_showUserLayers);
		item->hide();
		addItem(item);
		m_usermarkers[id] = item;
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

		if(changed.isEmpty() || changed.contains(Qt::DisplayRole))
			item->setText(um.data(Qt::DisplayRole).toString());

		if(changed.isEmpty() || changed.contains(canvas::UserCursorModel::LayerRole))
			item->setSubtext(um.data(canvas::UserCursorModel::LayerRole).toString());

		if(changed.isEmpty() || changed.contains(canvas::UserCursorModel::ColorRole))
			item->setColor(um.data(canvas::UserCursorModel::ColorRole).value<QColor>());

		if(changed.isEmpty() || changed.contains(canvas::UserCursorModel::VisibleRole)) {
			if(_showUserMarkers) {
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
	if(_showUserMarkers != show) {
		_showUserMarkers = show;
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

void CanvasScene::showUserLayers(bool show)
{
	if(_showUserLayers != show) {
		_showUserLayers = show;
		for(UserMarkerItem *item : m_usermarkers)
			item->setShowSubtext(show);
	}
}

void CanvasScene::showLaserTrails(bool show)
{
	_showLaserTrails = show;
	for(LaserTrailItem *i : m_lasertrails)
		i->hide();
}

void CanvasScene::setThickLaserTrails(bool thick)
{
	_thickLaserTrails = thick;
	for(LaserTrailItem *l : m_lasertrails) {
		l->setThick(thick);
	}
}

}
