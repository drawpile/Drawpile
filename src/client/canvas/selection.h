/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2018 Calle Laakkonen

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

#ifndef SELECTION_H
#define SELECTION_H

#include "core/blendmodes.h"
#include "../shared/net/message.h"

#include <QObject>
#include <QImage>
#include <QPolygonF>

namespace canvas {

/**
 * @brief This class represents a canvas area selection
 *
 * A selection can contain in image to be pasted.
 */
class Selection : public QObject
{
	Q_PROPERTY(QPolygonF shape READ shape WRITE setShape NOTIFY shapeChanged)
	Q_PROPERTY(QImage pasteImage READ pasteImage WRITE setPasteImage NOTIFY pasteImageChanged)
	Q_PROPERTY(int handleSize READ handleSize CONSTANT)
	Q_OBJECT
public:
	enum Handle {OUTSIDE, TRANSLATE, RS_TOPLEFT, RS_TOPRIGHT, RS_BOTTOMRIGHT, RS_BOTTOMLEFT, RS_TOP, RS_RIGHT, RS_BOTTOM, RS_LEFT};

	explicit Selection(QObject *parent=nullptr);

	//! Get the selection shape polygon
	QPolygonF shape() const { return m_shape; }

	//! Set the selection shape polygon. The shape is not closed.
	void setShape(const QPolygonF &shape);

	//! Set the selection shape from a rectangle. The shape is closed.
	void setShapeRect(const QRect &rect);

	//! Add a point to the selection polygon. The shape must be open
	void addPointToShape(const QPointF &point);

	//! Close the selection
	void closeShape();

	/**
	 * @brief Is the polygon closed?
	 *
	 * An open shape is a polygon that is still being drawn.
	 */
	bool isClosed() const { return m_closedPolygon; }

	/**
	 * @brief Get the adjustment handle (if any) at the given coordinates
	 * @param point point in canvas space
	 * @param zoom zoom factor (affects visible handle size)
	 */
	Handle handleAt(const QPointF &point, float zoom) const;

	/**
	 * @brief Save the current shape as the pre-adjustment shape
	 *
	 * The adjust* functions called afterwards replace the current shape
	 * with an adjusted version of the pre-adjustment shape.
	 *
	 * @param handle the handle to be adjusted
	 */
	void beginAdjustment(Handle handle);

	/**
	 * @brief Adjust shape geometry by dragging from one of the handles
	 *
	 * The selection can be translated and scaled by handle dragging.
	 * This replaces the current shape with a modified version of the shape saved
	 * when beginAdjustment was called.
	 *
	 * @param handle the handle that was dragged
	 * @param delta new handle position relative relative to its position when beginAdjustment was called
	 * @param keepAspect if true, aspect ratio is maintained
	 */
	void adjustGeometry(const QPoint &delta, bool keepAspect);

	/**
	 * @brief Adjust shape geometry by rotating it.
	 *
	 * This replaces the current shape with a modified version of the shape saved
	 * when beginAdjustment was called.
	 *
	 * @param angle radians to rotate the shape relative to when beginAdjustment was called
	 */
	void adjustRotation(float angle);

	/**
	 * @brief Adjust shape geometry by shearing it
	 *
	 * This replaces the current shape with a modified version of the shape saved
	 * when beginAdjustment was called.
	 *
	 * @param sh horizontal shearing factor
	 * @param sv vertical shearing factor
	 */
	void adjustShear(float sh, float sv);

	/**
	 * @brief Check if the selection is an axis-aligned rectangle
	 *
	 * Several optimizations can be used when this is true
	 */
	bool isAxisAlignedRectangle() const;

	//! Forget that the selection buffer came from the canvas (will be treated as a pasted image)
	void detachMove() { m_moveRegion = QPolygon(); }

	//! Did the selection buffer come from the canvas? (as opposed to an external pasted image)
	bool isMovedFromCanvas() const { return !m_moveRegion.isEmpty(); }

	QRect boundingRect() const { return m_shape.boundingRect().toRect(); }

	QImage shapeMask(const QColor &color, QRect *maskBounds) const;

	//! Set the image from pasting
	void setPasteImage(const QImage &image);

	/**
	 * @brief Set the preview image for moving a layer region
	 *
	 * The current shape will be remembered.
	 * @param image
	 * @param canvasSize the size of the canvas (selection rectangle is clipped to canvas bounds
	 * @param sourceLayerId the ID of the layer where the image came from
	 */
	void setMoveImage(const QImage &image, const QSize &canvasSize, int sourceLayerId);

	//! Get the image to be pasted (or the move preview)
	QImage pasteImage() const { return m_pasteImage; }

	/**
	 * @brief Generate the commands to paste or move an image
	 *
	 * If this selection contains a moved piece (setMoveImage called) calling this method
	 * will return the commands for a RegionMove.
	 * If the image came from outside (i.e the clipboard,) a PutImage command set will be returned
	 * instead.
	 *
	 * If a MoveRegion command is generated, the source layer ID set previously in setMoveImage is used
	 * instead of the given target layer
	 *
	 * @param contextId user ID for the commands
	 * @param layer target layer
	 * @return set of commands
	 */
	protocol::MessageList pasteOrMoveToCanvas(uint8_t contextId, int layer) const;

	/**
	 * @brief Generate the commands to fill the selection with solid color
	 *
	 * If this is an axis aligned rectangle, a FillRect command will be returned.
	 * Otherwise, a PutImage set will be generated.
	 *
	 * @param contextId user ID for the commands
	 * @param color fill color
	 * @param mode blending mode
	 * @param layer target layer
	 * @return set of commands
	 */
	protocol::MessageList fillCanvas(uint8_t contextId, const QColor &color, paintcore::BlendMode::Mode mode, int layer) const;

	/**
	 * @brief Get the size of the adjustment handles in pixels at 1:1 zoom level
	 */
	int handleSize() const { return 20; }

	//! Has the selection been moved or transformed?
	bool isTransformed() const;

public slots:
	//! Restore the original shape (but not the position)
	void resetShape();

	//! Restore the original shape and position
	void reset();

	//! Translate the selection directly (does not use pre-adjustment shape)
	void translate(const QPoint &offset);

	//! Scale the selection directly (does not use pre-adjustment shape)
	void scale(qreal x, qreal y);

signals:
	void shapeChanged(const QPolygonF &shape);
	void pasteImageChanged(const QImage &image);
	void closed();

private:
	void setPasteOrMoveImage(const QImage &image);
	void adjustScale(int dx1, int dy1, int dx2, int dy2);
	void saveShape();

	QPolygonF m_shape;
	QPolygonF m_originalShape;
	QPolygonF m_preAdjustmentShape;
	QPolygonF m_moveRegion;

	Handle m_adjustmentHandle;
	QPointF m_originalCenter;

	QImage m_pasteImage;

	int m_sourceLayerId = 0;

	bool m_closedPolygon = false;

};

}

#endif // SELECTION_H
