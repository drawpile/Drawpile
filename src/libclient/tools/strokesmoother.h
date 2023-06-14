// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef STROKESMOOTHER_H
#define STROKESMOOTHER_H

#include <QVector>

#include "libclient/canvas/point.h"

class StrokeSmoother
{
public:
	StrokeSmoother();

	/**
	 * If smoothing is enabled (smoothing > 0) or not.
	 */
	bool isActive() const;

	/**
	 * @brief Set smoothing strength
	 *
	 * The higher the value, the smoother the stroke will be. A strength of zero
	 * or less disables the smoother.
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
	void addPoint(const canvas::Point &point);

	/**
	 * @brief Is a smoothed point available?
	 *
	 * @return true if smoothPoint will return a valid value
	 */
	bool hasSmoothPoint() const;

	/**
	 * @brief Get the smoothed point
	 *
	 * @return smoothed point
	 * @pre hasSmoothPoint() == true
	 */
	canvas::Point smoothPoint() const;

	/**
	 * @brief Remove one point from the buffer, for ending a line
	 *
	 * @pre hasSmoothPoint() == true
	 */
	void removePoint();

	/**
	 * @brief Get the last point added to the smoother
	 * @return
	 */
	canvas::Point latestPoint() const { return at(0); }

	/**
	 * Add an offset to all stored points
	 *
	 * This is used to correct the brush position when the canvas
	 * is resized mid-stroke.
	 */
	void addOffset(const QPointF &offset);

private:
	canvas::Point at(int i) const;

	canvas::PointVector m_points;
	int m_pos;
	int m_count; ///< Number of actually sampled points in the buffer
};

#endif // STROKESMOOTHER_H
