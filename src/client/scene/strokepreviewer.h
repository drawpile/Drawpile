/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#ifndef STROKEPREVIEWER_H
#define STROKEPREVIEWER_H

#include <QPen>
#include <QQueue>

#include "core/point.h"
#include "core/brush.h"

namespace drawingboard {

class CanvasScene;

/**
 * @brief Base class for stroke preview managers
 */
class StrokePreviewer
{
public:
	StrokePreviewer(CanvasScene *scene) : _scene(scene) { }
	virtual ~StrokePreviewer() = default;

	virtual void startStroke(const paintcore::Brush &brush, const paintcore::Point &point, int layer) = 0;
	virtual void continueStroke(const paintcore::Point &point) = 0;
	virtual void takeStrokes(int count) = 0;
	virtual void endStroke() = 0;
	virtual void clear() = 0;

protected:
	CanvasScene * const _scene;
};

/**
 * @brief A no-operation stroke previewer
 */
class NopStrokePreviewer : public StrokePreviewer {
public:
	static NopStrokePreviewer *getInstance();

	void startStroke(const paintcore::Brush &, const paintcore::Point &, int ) {}
	void continueStroke(const paintcore::Point &) {}
	void endStroke() {}
	void takeStrokes(int) {}
	void clear() {}

private:
	NopStrokePreviewer() : StrokePreviewer(0) { }
};

/**
 * @brief Simple overlay item based stroke previewer
 */
class OverlayStrokePreviewer : public StrokePreviewer {
public:
	OverlayStrokePreviewer(CanvasScene *scene);
	~OverlayStrokePreviewer();

	void startStroke(const paintcore::Brush &brush, const paintcore::Point &point, int layer);
	void continueStroke(const paintcore::Point &point);
	void endStroke();
	void takeStrokes(int count);
	void clear();

protected:
	QPen _pen;

private:
	QList<QGraphicsLineItem*> _strokes, _cache;
	paintcore::Point _lastpoint;
};

/**
 * @brief An overlay previewer that approximates the brush style
 */
class ApproximateOverlayStrokePreviewer : public OverlayStrokePreviewer
{
public:
	ApproximateOverlayStrokePreviewer(CanvasScene *scene);

	void startStroke(const paintcore::Brush &brush, const paintcore::Point &point, int layer);
};

/**
 * @brief Stroke previewer that draws to a temporary layer using the real brush
 */
class TempLayerStrokePreviewer : public StrokePreviewer
{
public:
	TempLayerStrokePreviewer(CanvasScene *scene);
	~TempLayerStrokePreviewer();

	void startStroke(const paintcore::Brush &brush, const paintcore::Point &point, int layer);
	void continueStroke(const paintcore::Point &point);
	void endStroke();
	void takeStrokes(int count);
	void clear();

private:
	struct Stroke {
		Stroke(int l=0, int sl=0, int c=0) : layer(l), sublayer(sl), count(c), done(false) { }
		int layer;
		int sublayer;
		int count;
		bool done;
	};

	void removePreview(const Stroke &stroke);

	paintcore::Brush _brush;
	paintcore::Point _lastpoint;
	qreal _distance;
	QQueue<Stroke> _strokes;
	int _current_id;
};

}

#endif
