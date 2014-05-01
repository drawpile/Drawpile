/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
#include <QGraphicsLineItem>

#include "core/layerstack.h"
#include "core/layer.h"
#include "scene/strokepreviewer.h"
#include "scene/canvasscene.h"

namespace drawingboard {

static NopStrokePreviewer *NOP_INSTANCE;
NopStrokePreviewer *NopStrokePreviewer::getInstance() {
	if(!NOP_INSTANCE)
		NOP_INSTANCE = new NopStrokePreviewer();
	return NOP_INSTANCE;
}

OverlayStrokePreviewer::OverlayStrokePreviewer(CanvasScene *scene)
	: StrokePreviewer(scene)
{
	_pen.setColor(Qt::red);
	_pen.setWidth(1);
	_pen.setCosmetic(true);
}

OverlayStrokePreviewer::~OverlayStrokePreviewer()
{
	foreach(QGraphicsLineItem *p, _strokes)
		delete p;
	foreach(QGraphicsLineItem *p, _cache)
		delete p;
}

void OverlayStrokePreviewer::startStroke(const paintcore::Brush &brush, const paintcore::Point &point, int layer)
{
	Q_UNUSED(brush);
	Q_UNUSED(layer);

	_lastpoint = point;
	continueStroke(point);
}

void OverlayStrokePreviewer::continueStroke(const paintcore::Point &point)
{
	QGraphicsLineItem *s;
	if(_cache.isEmpty()) {
		s = new QGraphicsLineItem();
		_scene->addItem(s);
	} else {
		s = _cache.takeLast();
	}

	s->setPen(_pen);

	s->setLine(_lastpoint.x(), _lastpoint.y(), point.x(), point.y());
	s->show();
	_strokes.append(s);
	_lastpoint = point;

	_scene->resetPreviewClearTimer();
}

void OverlayStrokePreviewer::endStroke()
{
	// nothing to do
}

void OverlayStrokePreviewer::takeStrokes(int count)
{
	while(count-->0 && !_strokes.isEmpty()) {
		QGraphicsLineItem *s = _strokes.takeFirst();
		s->hide();
		_cache.append(s);
	}
}

void OverlayStrokePreviewer::clear()
{
	while(!_strokes.isEmpty()) {
		QGraphicsLineItem *s = _strokes.takeFirst();
		s->hide();
		_cache.append(s);
	}
	// Limit the size of the cache
	while(_cache.size() > 100) {
		delete _cache.takeLast();
	}
}

ApproximateOverlayStrokePreviewer::ApproximateOverlayStrokePreviewer(CanvasScene *scene)
	: OverlayStrokePreviewer(scene)
{
}

void ApproximateOverlayStrokePreviewer::startStroke(const paintcore::Brush &brush, const paintcore::Point &point, int layer)
{
	_pen = CanvasScene::penForBrush(brush);

	OverlayStrokePreviewer::startStroke(brush, point, layer);
}

TempLayerStrokePreviewer::TempLayerStrokePreviewer(CanvasScene *scene)
	: StrokePreviewer(scene), _current_id(0)
{
}

TempLayerStrokePreviewer::~TempLayerStrokePreviewer()
{
	clear();
}

bool TempLayerStrokePreviewer::pauseInput() const
{
	// Since we can't easily remove partial preview strokes, we work around it by
	// pausing input message handling for the duration of the stroke. This way the user
	// at least gets a clear view of what they are drawing, while they are drawing.

	// Exception: if the brush is idempotent, there is no need to remove the preview overlay
	return !_brush.isIdempotent();
}

void TempLayerStrokePreviewer::startStroke(const paintcore::Brush &brush, const paintcore::Point &point, int layer)
{
	_brush = brush;
	_lastpoint = point;
	_distance = 0;
	_strokes.append(Stroke(layer, --_current_id, 1));

	paintcore::Layer *l = _scene->layers()->getLayer(layer);
	if(l)
		l->dab(_current_id, _brush, point);

}

void TempLayerStrokePreviewer::continueStroke(const paintcore::Point &point)
{
	if(_strokes.isEmpty())
		return;

	Stroke &s = _strokes.last();
	paintcore::Layer *l = _scene->layers()->getLayer(s.layer);

	if(l)
		l->drawLine(s.sublayer, _brush, _lastpoint, point, _distance);

	_lastpoint = point;
	++s.count;

	_scene->resetPreviewClearTimer();
}

void TempLayerStrokePreviewer::endStroke()
{
	if(_strokes.isEmpty())
		return;

	Stroke &s = _strokes.last();
	s.done = true;
	if(s.count<=0) {
		removePreview(s);
		_strokes.removeLast();
	}
}

void TempLayerStrokePreviewer::takeStrokes(int count)
{
	if(_strokes.isEmpty())
		return;

	Stroke &s = _strokes.first();

	s.count -= count;
	if(s.count<=0) {
		s.count = 0;
		removePreview(s);
		if(s.done)
			_strokes.removeFirst();
	}
}

void TempLayerStrokePreviewer::clear()
{
	while(!_strokes.empty())
		removePreview(_strokes.takeLast());
}

void TempLayerStrokePreviewer::removePreview(const Stroke &stroke)
{
	paintcore::Layer *l = _scene->layers()->getLayer(stroke.layer);
	if(l)
		l->removeSublayer(stroke.sublayer);
}

}
