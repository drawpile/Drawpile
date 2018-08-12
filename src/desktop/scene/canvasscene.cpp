/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2017 Calle Laakkonen

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
	: QGraphicsScene(parent), m_image(nullptr), m_model(nullptr),
	  m_selection(nullptr),
	  _showAnnotationBorders(false), _showAnnotations(true),
	  m_showUserMarkers(true), m_showUserLayers(true), m_showUserAvatars(true), m_showLaserTrails(true)
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
	delete m_image;
}

/**
 * This prepares the canvas for new drawing commands.
 * @param myid the context id of the local user
 */
void CanvasScene::initCanvas(canvas::CanvasModel *model)
{
	delete m_image;
	onSelectionChanged(nullptr);

	m_model = model;

	m_image = new CanvasItem(m_model->layerStack());

	connect(m_model->layerStack(), &paintcore::LayerStack::resized, this, &CanvasScene::handleCanvasResize);

	paintcore::AnnotationModel *anns = m_model->layerStack()->annotations();
	connect(anns, &paintcore::AnnotationModel::rowsInserted, this, &CanvasScene::annotationsAdded);
	connect(anns, &paintcore::AnnotationModel::dataChanged, this, &CanvasScene::annotationsChanged);
	connect(anns, &paintcore::AnnotationModel::rowsAboutToBeRemoved, this, &CanvasScene::annotationsRemoved);
	connect(anns, &paintcore::AnnotationModel::modelReset, this, &CanvasScene::annotationsReset);

	canvas::UserCursorModel *cursors = m_model->userCursors();
	connect(cursors, &canvas::UserCursorModel::rowsInserted, this, &CanvasScene::userCursorAdded);
	connect(cursors, &canvas::UserCursorModel::dataChanged, this, &CanvasScene::userCursorChanged);
	connect(cursors, &canvas::UserCursorModel::rowsAboutToBeRemoved, this, &CanvasScene::userCursorRemoved);

	canvas::LaserTrailModel *lasers = m_model->laserTrails();
	connect(lasers, &canvas::LaserTrailModel::rowsInserted, this, &CanvasScene::laserAdded);
	connect(lasers, &canvas::LaserTrailModel::dataChanged, this, &CanvasScene::laserChanged);
	connect(lasers, &canvas::LaserTrailModel::rowsAboutToBeRemoved, this, &CanvasScene::laserRemoved);

	connect(m_model, &canvas::CanvasModel::selectionChanged, this, &CanvasScene::onSelectionChanged);

	addItem(m_image);

	annotationsReset();

	for(UserMarkerItem *i : m_usermarkers)
		delete i;
	m_usermarkers.clear();
	
	QList<QRectF> regions;
	regions.append(sceneRect());
	emit changed(regions);
}

void CanvasScene::showCanvas()
{
	if(m_image)
		m_image->setVisible(true);
}

void CanvasScene::hideCanvas()
{
	if(m_image)
		m_image->setVisible(false);
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
	for(QGraphicsItem *item : items()) {
		if(item->type() == AnnotationItem::Type)
			item->setVisible(show);
	}
}

void CanvasScene::showAnnotationBorders(bool hl)
{
	_showAnnotationBorders = hl;
	for(QGraphicsItem *item : items()) {
		if(item->type() == AnnotationItem::Type)
			static_cast<AnnotationItem*>(item)->setShowBorder(hl);
	}
}

void CanvasScene::handleCanvasResize(int xoffset, int yoffset, const QSize &oldsize)
{
	if(!m_image)
		return;
	QRectF bounds = m_image->boundingRect();

	// Include some empty space around the canvas to make working
	// near the borders easier.
	const float wPadding = 300;
	const float hPadding = 300;

	setSceneRect(bounds.adjusted(-wPadding, -hPadding, wPadding, hPadding));
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

void CanvasScene::activeAnnotationChanged(int id)
{
	for(QGraphicsItem *i : items()) {
		AnnotationItem *item = qgraphicsitem_cast<AnnotationItem*>(i);
		if(item)
			item->setHighlight(item->id() == id);
	}
}

void CanvasScene::annotationsAdded(const QModelIndex&, int first, int last)
{
	for(int i=first;i<=last;++i) {
		const QModelIndex a = m_model->layerStack()->annotations()->index(i);
		const int id = a.data(paintcore::AnnotationModel::IdRole).toInt();
		if(getAnnotationItem(id)) {
			qWarning("Annotation item already exists for ID %#x", id);
			continue;
		}

		AnnotationItem *item = new AnnotationItem(id);
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
		else
			qWarning("Could not find annotation item %#x for deletion", id);
	}

}

void CanvasScene::annotationsChanged(const QModelIndex &first, const QModelIndex &last, const QVector<int> &changed)
{
	const int ifirst = first.row();
	const int ilast = last.row();
	for(int i=ifirst;i<=ilast;++i) {
		const QModelIndex a = m_model->layerStack()->annotations()->index(i);
		const int id = a.data(paintcore::AnnotationModel::IdRole).toInt();
		AnnotationItem *item = getAnnotationItem(id);

		if(!item) {
			qWarning("Could not find annotation item %#x for update", id);
			continue;
		}

		if(changed.isEmpty() || changed.contains(Qt::DisplayRole))
			item->setText(a.data(Qt::DisplayRole).toString());

		if(changed.isEmpty() || changed.contains(paintcore::AnnotationModel::RectRole))
			item->setGeometry(a.data(paintcore::AnnotationModel::RectRole).toRect());

		if(changed.isEmpty() || changed.contains(paintcore::AnnotationModel::BgColorRole))
			item->setColor(a.data(paintcore::AnnotationModel::BgColorRole).value<QColor>());

		if(changed.isEmpty() || changed.contains(paintcore::AnnotationModel::VAlignRole))
			item->setValign(a.data(paintcore::AnnotationModel::VAlignRole).toInt());
	}
}

void CanvasScene::annotationsReset()
{
	// Clear out any old annotation items
	for(QGraphicsItem *item : items()) {
		if(item->type() == AnnotationItem::Type) {
			delete item;
		}
	}

	if(!m_model->layerStack()->annotations()->isEmpty()) {
		annotationsAdded(QModelIndex(), 0, m_model->layerStack()->annotations()->rowCount()-1);
	}
}

void CanvasScene::laserAdded(const QModelIndex&, int first, int last)
{
	if(!m_image)
		return;

	// Don't add new lasers when canvas is hidden
	if(!m_image->isVisible())
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
	if(!m_image || !m_image->isVisible())
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
