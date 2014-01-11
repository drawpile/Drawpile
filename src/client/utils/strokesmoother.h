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
#ifndef STROKESMOOTHER_H
#define STROKESMOOTHER_H

#include <QVector>

#include "core/point.h"

class StrokeSmoother
{
public:
	StrokeSmoother();

	/**
	 * @brief Set smoothing strength
	 *
	 * The higher the value, the smoother the stroke will be.
	 * Strength should be greater than 1.
	 * @param strength
	 */
	void setSmoothing(int strength);

	/**
	 * @brief Reset smoother for a new stroke
	 */
	void reset();

	/**
	 * @brief Add a point to the smoother
	 * @param point
	 */
	void addPoint(const paintcore::Point &point);

	/**
	 * @brief Is a smoothed point available?
	 *
	 * @return true if smoothPoint will return a valid value
	 */
	bool hasSmoothPoint() const;

	/**
	 * @brief Is this the first available smoothed point
	 *
	 * @return true if this is the first smoothed point
	 */
	bool isFirstSmoothPoint() const { return _firstsmooth; }

	/**
	 * @brief Get the smoothed point
	 *
	 * @return smoothed point
	 * @pre hasSmoothPoint() == true
	 */
	paintcore::Point smoothPoint() const;

private:
	paintcore::Point at(int i) const;

	QVector<paintcore::Point> _points;
	int _pos;
	int _count;
	bool _firstsmooth;
};

#endif // STROKESMOOTHER_H
