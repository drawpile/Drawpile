/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#include "canvastransform.h"

#include <QMatrix4x4>

CanvasTransform::CanvasTransform(QObject *parent)
	: QQuickTransform(parent),
	  m_x(0), m_y(0), m_scale(1), m_rotation(0), m_flip(false), m_mirror(false)
{
}

void CanvasTransform::setOrigin(const QVector3D &origin)
{
	if(origin != m_origin) {
		m_origin = origin;
		update();
		emit originChanged(origin);
	}
}

void CanvasTransform::setTranslateX(qreal x)
{
	if(x != m_x) {
		m_x = x;
		update();
		emit xChanged(x);
	}
}

void CanvasTransform::setTranslateY(qreal y)
{
	if(y != m_y) {
		m_y = y;
		update();
		emit yChanged(y);
	}
}

void CanvasTransform::setScale(qreal s)
{
	if(s != m_scale) {
		m_scale = s;
		update();
		emit scaleChanged(s);
	}
}

void CanvasTransform::setRotation(qreal r)
{
	if(r != m_rotation) {
		m_rotation = r;
		update();
		emit rotationChanged(r);
	}
}

void CanvasTransform::setFlip(bool flip)
{
	if(flip != m_flip) {
		m_flip = flip;
		update();
		emit flipped(flip);
	}
}

void CanvasTransform::setMirror(bool mirror)
{
	if(mirror != m_mirror) {
		m_mirror = mirror;
		update();
		emit mirrored(mirror);
	}
}

void CanvasTransform::applyTo(QMatrix4x4 *matrix) const
{
	matrix->translate(m_origin);
	matrix->translate(m_x * m_scale, m_y * m_scale);
	matrix->scale(
		m_mirror ? -m_scale : m_scale,
		m_flip ? -m_scale : m_scale,
		1);
	matrix->rotate(m_rotation, 0, 0, 1);
	matrix->translate(-m_origin);
}
