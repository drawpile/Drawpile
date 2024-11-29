// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_REFERENCEVIEW_H
#define DESKTOP_WIDGETS_REFERENCEVIEW_H
#include <QGraphicsView>
#include <QPointF>
#include <QTransform>

class QGraphicsPixmapItem;
class QGraphicsView;
class QImage;

namespace widgets {

class ReferenceView final : public QGraphicsView {
	Q_OBJECT
public:
	explicit ReferenceView(QWidget *parent = nullptr);

	bool hasImage() const { return m_item != nullptr; }
	void setImage(const QImage &img);

	const QPointF &pos() const { return m_pos; }
	void scrollBy(int x, int y);
	void scrollByF(qreal x, qreal y);

	qreal zoom() const { return m_zoom; }
	void setZoom(qreal zoom);
	void resetZoom();
	void zoomToFit();

	static qreal zoomMin();
	static qreal zoomMax();

signals:
	void zoomChanged(qreal zoom);

protected:
	void scrollContentsBy(int dx, int dy) override;

private:
	qreal actualZoom() const { return actualZoomFor(m_zoom); }
	qreal actualZoomFor(qreal zoom) const { return zoom / devicePixelRatioF(); }
	void updateZoomAt(qreal zoom, const QPointF &pos);

	void updateTransform();
	QTransform calculateTransform() const;
	QTransform calculateTransformFrom(const QPointF &pos, qreal zoom) const;

	void updatePosBounds();
	void clampPos();

	void updateScrollBars();

	QGraphicsPixmapItem *m_item = nullptr;
	QPointF m_pos;
	qreal m_zoom = 1.0;
	QRectF m_posBounds;
	bool m_scrollBarsAdjusting = false;
};

}

#endif
