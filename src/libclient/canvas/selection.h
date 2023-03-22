// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SELECTION_H
#define SELECTION_H

extern "C" {
#include <dpmsg/blend_mode.h>
}

#include "libclient/drawdance/message.h"

#include <QObject>
#include <QImage>
#include <QPolygonF>

namespace canvas {

/**
 * @brief This class represents a canvas area selection
 *
 * A selection can contain in image to be pasted.
 */
class Selection final : public QObject
{
	Q_PROPERTY(QPolygonF shape READ shape WRITE setShape NOTIFY shapeChanged)
	Q_PROPERTY(QImage pasteImage READ pasteImage WRITE setPasteImage NOTIFY pasteImageChanged)
	Q_PROPERTY(int handleSize READ handleSize CONSTANT)
	Q_OBJECT
public:
	enum class Handle {Outside, Center, TopLeft, TopRight, BottomRight, BottomLeft, Top, Right, Bottom, Left};
	enum class AdjustmentMode { Scale, Rotate, Distort };

	explicit Selection(QObject *parent=nullptr);

	//! Get the selection shape polygon
	QPolygonF shape() const { return m_shape; }

	//! Set the selection shape polygon.
	void setShape(const QPolygonF &shape);

	//! Set the selection shape from a rectangle.
	void setShapeRect(const QRect &rect);

	//! Add a point to the selection polygon. The shape must be open
	void addPointToShape(const QPointF &point);

	//! Close the selection
	void closeShape();

	/**
	 * Close the selection and clip it to the given rectangle
	 * @param clipRect rectangle that represents the working area
	 * @return false if selection was entirely outside the clip rectangle
	 */
	bool closeShape(const QRectF &clipRect);

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
	Handle handleAt(const QPointF &point, qreal zoom) const;

	/**
	 * Select the mode for the adjustment handles
	 */
	void setAdjustmentMode(AdjustmentMode mode);

	AdjustmentMode adjustmentMode() const { return m_adjustmentMode; }

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
	 * @param start the starting coordinate
	 * @param point current pointer position
	 * @param constrain apply constraint (aspect ratio or discrete angle)
	 */
	void adjustGeometry(const QPointF &start, const QPointF &point, bool constrain);

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

	//! Get the shape at which the move started
	QPolygonF moveSourceRegion() const { return m_moveRegion; }

	QRect boundingRect() const { return m_shape.boundingRect().toRect(); }

	QImage shapeMask(const QColor &color, QRect *maskBounds) const;

	//! Set the pasted image
	void setPasteImage(const QImage &image);

	/**
	 * @brief Set the preview image for moving a layer region
	 *
	 * The current shape will be remembered.
	 * @param image
	 * @param imageRect the rectangle from which the image was captured
	 * @param canvasSize the size of the canvas (selection rectangle is clipped to canvas bounds
	 * @param sourceLayerId the ID of the layer where the image came from
	 */
	void setMoveImage(const QImage &image, const QRect &imageRect, const QSize &canvasSize, int sourceLayerId);

	//! Get the image to be pasted (or the move preview)
	QImage pasteImage() const { return m_pasteImage; }

	//! Get the image to be pasted with the current transformation applied
	QImage transformedPasteImage() const;

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
	 * @param buffer vector to put the commands into
	 * @param contextId user ID for the commands
	 * @param layer target layer
	 * @param interpolation how to interpolate, one of DP_MSG_MOVE_REGION_MODE_*
	 * @return set of commands
	 */
	bool pasteOrMoveToCanvas(drawdance::MessageList &buffer, uint8_t contextId, int layer, int interpolation) const;

	/**
	 * @brief Generate the commands to fill the selection with solid color
	 *
	 * If this is an axis aligned rectangle, a FillRect command will be returned.
	 * Otherwise, a PutImage set will be generated.
	 *
	 * @param buffer vector to put the commands into
	 * @param contextId user ID for the commands
	 * @param color fill color
	 * @param mode blending mode
	 * @param layer target layer
	 * @return set of commands
	 */
	bool fillCanvas(drawdance::MessageList &buffer, uint8_t contextId, const QColor &color, DP_BlendMode mode, int layer) const;

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
	void adjustmentModeChanged(AdjustmentMode mode);

private:
	void adjustGeometryScale(const QPoint &delta, bool keepAspect);
	void adjustGeometryRotate(const QPointF &start, const QPointF &point, bool constrain);
	void adjustGeometryDistort(const QPointF &delta);
	void adjustTranslation(const QPointF &start, const QPointF &point);
	void adjustTranslation(const QPointF &delta);
	void adjustScale(qreal dx1, qreal dy1, qreal dx2, qreal dy2);
	void adjustRotation(qreal angle);
	void adjustShear(qreal sh, qreal sv);
	void adjustDistort(const QPointF &delta, int corner1, int corner2 = -1);

	void saveShape();

	bool isOnlyTranslated() const;

	QPolygonF m_shape;
	QPolygonF m_originalShape;
	QPolygonF m_preAdjustmentShape;
	QPolygonF m_moveRegion;

	AdjustmentMode m_adjustmentMode = AdjustmentMode::Scale;
	Handle m_adjustmentHandle = Handle::Outside;
	QPointF m_originalCenter;

	QImage m_pasteImage;

	int m_sourceLayerId = 0;

	bool m_closedPolygon = false;
};

}

#endif // SELECTION_H
