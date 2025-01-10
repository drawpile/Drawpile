// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_REFERENCEVIEW_H
#define DESKTOP_WIDGETS_REFERENCEVIEW_H
#include <QGraphicsView>
#include <QPointF>
#include <QTransform>

class QColor;
class QGraphicsView;
class QImage;
class QMimeData;

namespace widgets {

class ReferenceView final : public QGraphicsView {
	Q_OBJECT
public:
	enum class InteractionMode { None, Pan, Pick };

	explicit ReferenceView(QWidget *parent = nullptr);

	bool hasImage() const { return m_item != nullptr; }
	void setImage(const QImage &img);

	const QPointF &pos() const { return m_pos; }
	void scrollBy(int x, int y);
	void scrollByF(qreal x, qreal y);

	qreal zoom() const { return m_zoom; }
	void setZoom(qreal zoom);
	void setZoomAt(qreal zoom, const QPointF &point);
	void zoomStepsAt(int steps, const QPointF &point);
	void resetZoom();
	void zoomToFit();

	void setInteractionMode(InteractionMode interactionMode);

	static qreal zoomMin();
	static qreal zoomMax();

signals:
	void colorPicked(const QColor &color);
	void zoomChanged(qreal zoom);
	void openRequested();
	void imageDropped(const QImage &img);
	void pathDropped(const QString &path);

protected:
	void resizeEvent(QResizeEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dragLeaveEvent(QDragLeaveEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	void scrollContentsBy(int dx, int dy) override;

private:
	class ReferenceItem;

	static constexpr qreal SCROLL_RATIO = -1.0 / 30.0;

	void pickColorAt(const QPointF &posf);

	bool canHandleDrop(const QMimeData *mimeData);

	qreal actualZoom() const { return actualZoomFor(m_zoom); }
	qreal actualZoomFor(qreal zoom) const { return zoom / devicePixelRatioF(); }
	void updateZoomAt(qreal zoom, const QPointF &pos);

	void updateCanvasTransform();
	QTransform calculateCanvasTransform() const;
	QTransform
	calculateCanvasTransformFrom(const QPointF &pos, qreal zoom) const;

	QPointF mapToCanvas(const QPoint &point) const;
	QPointF mapToCanvasF(const QPointF &point) const;
	QTransform toCanvasTransform() const;

	void updatePosBounds();
	void clampPos();

	void updateRenderHints();
	void updateScrollBars();

	void updateCursor(Qt::KeyboardModifiers mods);

	InteractionMode
	getEffectiveInteractionMode(Qt::KeyboardModifiers mods) const;

	InteractionMode
	getInteractionModeFor(Qt::KeyboardModifiers mods, bool invert) const;

	ReferenceItem *m_item = nullptr;
	QPointF m_pos;
	qreal m_zoom = 1.0;
	QRectF m_posBounds;
	InteractionMode m_interactionMode = InteractionMode::Pick;
	InteractionMode m_activeInteractionMode = InteractionMode::None;
	QPointF m_dragLastPoint;
	bool m_smoothing = false;
	bool m_transformAdjusting = false;
};

}

#endif
