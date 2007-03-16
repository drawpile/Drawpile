/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen, based on the GTK+ color selector (C) The Free Software Foundation

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

#include <QPainter>
#include <QPaintEvent>
#include <cmath>

#include "colortriangle.h"

#include "../../config.h"

#ifndef DESIGNER_PLUGIN
namespace widgets {
#endif

ColorTriangle::ColorTriangle(QWidget *parent,const QColor& color)
	: QWidget(parent), mode_(NODRAG)
{
	color.getHsvF(&hue_,&saturation_,&value_);
	if(hue_<0)
		hue_ = 0;
	setAcceptDrops(true);
	updateColorTriangle();
}

void ColorTriangle::updateColorTriangle()
{
	diameter_ = qMin(width(), height());
	center_ = diameter_/2.0;
	outer_ =  center_ - 2;
	inner_ = outer_ - 15; // ring width
	innert_ = inner_ - 5; // triangle point

	xoff_ = width() / 2 - diameter_/2;
	yoff_ = height() / 2 - diameter_/2;

	updateVertices();
	makeRing();
	triangle_ = QImage(diameter_, diameter_, QImage::Format_ARGB32_Premultiplied);
	makeTriangle();
}

int ColorTriangle::hue() const
{
	return qRound(hue_ * 359);
}

int ColorTriangle::saturation() const
{
	return qRound(saturation_ * 255);
}

int ColorTriangle::value() const
{
	return qRound(value_ * 255);
}

QColor ColorTriangle::color() const
{
	return QColor::fromHsvF(hue_, saturation_, value_);
}

void ColorTriangle::setColor(const QColor& color)
{
	qreal h = hue_;
	color.getHsvF(&hue_,&saturation_,&value_);
	if(hue_<0)
		hue_ = h;
	updateVertices();
	makeTriangle();
	update();
}

static inline qreal INTENSITY(const QColor& color) {
	return color.redF() * 0.30 + color.greenF() * 0.59 + color.blueF() * 0.11;
}

/**
 * Draw widget contents on screen
 * @param event event info
 */
void ColorTriangle::paintEvent(QPaintEvent *event)
{
	(void)event;
	QPainter painter(this);
	QPoint offset(xoff_, yoff_);
	painter.drawPixmap(offset, wheel_);
	painter.drawImage(offset, triangle_);

	QPen pen(Qt::black);
	pen.setWidth(2);
	painter.setPen(pen);

	// Markers
	qreal angle = hueAngle();
	int x = int(center_ + cos(angle) * (inner_+(outer_-inner_)/2.0) + 0.5);
	int y = int(center_ - sin(angle) * (inner_+(outer_-inner_)/2.0) + 0.5);
	painter.drawEllipse(x-4+xoff_, y-4+yoff_, 8, 8);

	x = int(sx_ + (vx_-sx_) * value_ + (hx_-vx_) * saturation_ * value_ + 0.5);
	y = int(sy_ + (vy_-sy_) * value_ + (hy_-vy_) * saturation_ * value_ + 0.5);

	if(INTENSITY(color()) < 0.5) {
		pen.setColor(Qt::white);
		painter.setPen(pen);
	}
	painter.drawEllipse(x-4+xoff_, y-4+yoff_, 8, 8);

}

/**
 * Update color wheel and triangle on widget resize
 * @param event event info
 */
void ColorTriangle::resizeEvent(QResizeEvent *event)
{
	(void)event;
	updateColorTriangle();
}

/**
 * @brief Handle mouse press
 * @param event event info
 */
void ColorTriangle::mousePressEvent(QMouseEvent *event)
{
	QPoint pos = event->pos() - QPoint(xoff_,yoff_);
	if(isInRing(pos.x(), pos.y())) {
		setHue(pos.x(), pos.y());
		makeTriangle();
		update();
		mode_ = DRAGHUE;
		emit colorChanged(color());
	} else if(isInTriangle(pos.x(), pos.y())) {
		setSv(pos.x(), pos.y());
		update();
		mode_ = DRAGSV;
		emit colorChanged(color());
	}
}

/**
 * @brief Handle drags
 * @param event event info
 */
void ColorTriangle::mouseMoveEvent(QMouseEvent *event)
{
	QPoint pos = event->pos() - QPoint(xoff_,yoff_);
	if(mode_==DRAGHUE) {
		setHue(pos.x(), pos.y());
		makeTriangle();
		update();
		emit colorChanged(color());
	} else if(mode_==DRAGSV) {
		setSv(pos.x(), pos.y());
		update();
		emit colorChanged(color());
	}
}

/**
 * @brief Stop dragging
 * @param event event info
 */
void ColorTriangle::mouseReleaseEvent(QMouseEvent *event)
{
	(void)event;
	mode_ = NODRAG;
}

/**
 * @brief accept color drops
 * @param event event info
 */
void ColorTriangle::dragEnterEvent(QDragEnterEvent *event)
{
        if(event->mimeData()->hasFormat("application/x-color"))
                event->acceptProposedAction();
}

/**
 * @brief handle color drops
 * @param event event info
 */
void ColorTriangle::dropEvent(QDropEvent *event)
{
        QColor col = qvariant_cast<QColor>(event->mimeData()->colorData());
		setColor(col);
		emit colorChanged(col);
}

/**
 */
void ColorTriangle::updateVertices()
{
	qreal angle = hueAngle();

	hx_ = int (center_ + cos (angle) * innert_ + 0.5);
	hy_ = int (center_ - sin (angle) * innert_ + 0.5);
	sx_ = int (center_ + cos (angle + 2.0 * M_PI / 3.0) * innert_ + 0.5);
	sy_ = int (center_ - sin (angle + 2.0 * M_PI / 3.0) * innert_ + 0.5);
	vx_ = int (center_ + cos (angle + 4.0 * M_PI / 3.0) * innert_ + 0.5);
	vy_ = int (center_ - sin (angle + 4.0 * M_PI / 3.0) * innert_ + 0.5);
}

/**
 * @param x x coordinate
 * @param y y coordinate
 * @return true if coordinate is inside hue ring
 */
bool ColorTriangle::isInRing(qreal x, qreal y) const
{
	qreal dx, dy, dist;

	dx = x - center_;
	dy = center_ - y;
	dist = dx * dx + dy * dy;

	return (dist >= inner_ * inner_ && dist <= outer_ * outer_);
}

bool ColorTriangle::isInTriangle(qreal x, qreal y) const
{
	double det, s, v;

	det = (vx_ - sx_) * (hy_ - sy_) - (vy_ - sy_) * (hx_ - sx_);

	s = ((x - sx_) * (hy_ - sy_) - (y - sy_) * (hx_ - sx_)) / det;
	v = ((vx_ - sx_) * (y - sy_) - (vy_ - sy_) * (x - sx_)) / det;

	return (s >= 0.0 && v >= 0.0 && s + v <= 1.0);
}

qreal ColorTriangle::hueAngle() const
{
	return hue_ * 2.0 * M_PI;
}

void ColorTriangle::setHue(qreal x, qreal y)
{
	double dx = x - center_;
	double dy = center_ - y;

	double angle = atan2 (dy, dx);
	if (angle < 0.0)
	angle += 2.0 * M_PI;

	hue_ = angle / (2.0 * M_PI);

	updateVertices();
}

void ColorTriangle::setSv(qreal x, qreal y)
{
	qreal hx = hx_ - center_;
	qreal hy = center_ - hy_;
	qreal sx = sx_ - center_;
	qreal sy = center_ - sy_;
	qreal vx = vx_ - center_;
	qreal vy = center_ - vy_;
	x -= center_;
	y = center_ - y;

	if (vx * (x - sx) + vy * (y - sy) < 0.0) {
		saturation_ = 1.0;
		value_ = (((x - sx) * (hx - sx) + (y - sy) * (hy-sy))
				/ ((hx - sx) * (hx - sx) + (hy - sy) * (hy - sy)));

		if (value_ < 0.0)
			value_ = 0.0;
		else if (value_ > 1.0)
			value_ = 1.0;
	} else if (hx * (x - sx) + hy * (y - sy) < 0.0) {
		saturation_ = 0.0;
		value_ = (((x - sx) * (vx - sx) + (y - sy) * (vy - sy))
				/ ((vx - sx) * (vx - sx) + (vy - sy) * (vy - sy)));

		if (value_ < 0.0)
			value_ = 0.0;
		else if (value_ > 1.0)
			value_ = 1.0;
	} else if (sx * (x - hx) + sy * (y - hy) < 0.0) {
		value_ = 1.0;
		saturation_ = (((x - vx) * (hx - vx) + (y - vy) * (hy - vy)) /
				((hx - vx) * (hx - vx) + (hy - vy) * (hy - vy)));

		if (saturation_ < 0.0)
			saturation_ = 0.0;
		else if (saturation_ > 1.0)
			saturation_ = 1.0;
	} else {
		value_ = (((x - sx) * (hy - vy) - (y - sy) * (hx - vx))
				/ ((vx - sx) * (hy - vy) - (vy - sy) * (hx - vx)));

		if (value_<= 0.0) {
			value_ = 0.0;
			saturation_ = 0.0;
		} else {
			if (value_ > 1.0)
			value_ = 1.0;

			if (fabs (hy - vy) < fabs (hx - vx))
				saturation_ = (x - sx - value_ * (vx - sx)) / (value_ * (hx - vx));
			else
				saturation_ = (y - sy - value_ * (vy - sy)) / (value_ * (hy - vy));

			if (saturation_ < 0.0)
				saturation_ = 0.0;
			else if (saturation_ > 1.0)
				saturation_ = 1.0;
		}
	}
}


void ColorTriangle::makeRing()
{
	QImage ring(diameter_, diameter_, QImage::Format_ARGB32_Premultiplied);
	uchar *buf = ring.bits();

	qreal inner2 = (inner_-1) * (inner_-1);
	qreal outer2 = (outer_-1) * (outer_-1);

	for (int yy = 0; yy < diameter_; yy++) {
		qreal dy = -(yy - center_);

		for (int xx = 0; xx < diameter_; xx++) {
			qreal dx = xx - center_;

			qreal dist = dx * dx + dy * dy;
			if (dist < inner2 || dist > outer2) {
				*buf++ = 0;
				*buf++ = 0;
				*buf++ = 0;
				*buf++ = 0;
				continue;
			}

			qreal angle = atan2 (dy, dx);
			if (angle < 0.0)
				angle += 2.0 * M_PI;

			QColor color = QColor::fromHsvF(
					angle / (2.0 * M_PI),
					1.0,
					1.0
					);

#ifdef IS_BIG_ENDIAN
			*buf++ = 255;
			*buf++ = color.red();
			*buf++ = color.green();
			*buf++ = color.blue();
#else
			*buf++ = color.blue();
			*buf++ = color.green();
			*buf++ = color.red();
			*buf++ = 255;
#endif
		}
	}
	wheel_ = QPixmap::fromImage(ring);
}

static inline int LERP(int a,int b,int v1,int v2, int i)
{
	if(v2-v1 != 0)
		return a + (b-a) * (i-v1)/ (v2 - v1);
	else
		return a;
}

void ColorTriangle::makeTriangle()
{
	int xl, xr, rl, rr, gl, gr, bl, br; /* Scanline data */
	int xx, yy;

	int x1 = hx_;
	int y1 = hy_;
	QColor color = QColor::fromHsvF(hue_, 1.0, 1.0);
	int r1 = color.red();
	int g1 = color.green();
	int b1 = color.blue();

	int x2 = sx_;
	int y2 = sy_;
	color = QColor::fromHsvF(hue_,1.0,0.0);
	int r2 = color.red();
	int g2 = color.green();
	int b2 = color.blue();

	int x3 = vx_;
	int y3 = vy_;
	color = QColor::fromHsvF(hue_,0.0,1.0);
	int r3 = color.red();
	int g3 = color.green();
	int b3 = color.blue();

	if (y2 > y3) {
		qSwap (x2, x3);
		qSwap (y2, y3);
		qSwap (r2, r3);
		qSwap (g2, g3);
		qSwap (b2, b3);
	}

	if (y1 > y3) {
		qSwap (x1, x3);
		qSwap (y1, y3);
		qSwap (r1, r3);
		qSwap (g1, g3);
		qSwap (b1, b3);
	}

	if (y1 > y2) {
		qSwap (x1, x2);
		qSwap (y1, y2);
		qSwap (r1, r2);
		qSwap (g1, g2);
		qSwap (b1, b2);
	}

	// Shade the triangle
	uchar *buf = triangle_.bits();

	for (yy = 0; yy < diameter_; ++yy) {
		if (yy < y1 || yy > y3) {
			for (xx = 0; xx < diameter_; ++xx) {
				*buf++ = 0;
				*buf++ = 0;
				*buf++ = 0;
				*buf++ = 0;
			}
		} else {
			if (yy < y2) {
				xl = LERP (x1, x2, y1, y2, yy);

				rl = LERP (r1, r2, y1, y2, yy);
				gl = LERP (g1, g2, y1, y2, yy);
				bl = LERP (b1, b2, y1, y2, yy);
			} else {
				xl = LERP (x2, x3, y2, y3, yy);

				rl = LERP (r2, r3, y2, y3, yy);
				gl = LERP (g2, g3, y2, y3, yy);
				bl = LERP (b2, b3, y2, y3, yy);
			}

			xr = LERP (x1, x3, y1, y3, yy);

			rr = LERP (r1, r3, y1, y3, yy);
			gr = LERP (g1, g3, y1, y3, yy);
			br = LERP (b1, b3, y1, y3, yy);

			if (xl > xr) {
				qSwap (xl, xr);
				qSwap (rl, rr);
				qSwap (gl, gr);
				qSwap (bl, br);
			}

			for (xx = 0; xx < diameter_; ++xx) {
				if (xx < xl || xx > xr) {
					*buf++ = 0;
					*buf++ = 0;
					*buf++ = 0;
					*buf++ = 0;
				} else {
#ifdef IS_BIG_ENDIAN
					*buf++ = 0xff;
					*buf++ = LERP (rl, rr, xl, xr, xx);
					*buf++ = LERP (gl, gr, xl, xr, xx);
					*buf++ = LERP (bl, br, xl, xr, xx);
#else
					*buf++ = LERP (bl, br, xl, xr, xx);
					*buf++ = LERP (gl, gr, xl, xr, xx);
					*buf++ = LERP (rl, rr, xl, xr, xx);
					*buf++ = 0xff;
#endif
				}
			}
		}
	}
}

#ifndef DESIGNER_PLUGIN
}
#endif

