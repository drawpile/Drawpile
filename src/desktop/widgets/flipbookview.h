// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FLIPBOOKVIEW_H
#define FLIPBOOKVIEW_H

#include <QWidget>

class QRubberBand;

class FlipbookView final : public QWidget
{
	Q_OBJECT
public:
	explicit FlipbookView(QWidget *parent = nullptr);

	bool isUpscaling() const { return m_upscale; }

public slots:
	void setPixmap(const QPixmap &pixmap);
	void setUpscaling(bool upscale);
	void startCrop();

signals:
	/**
	 * @brief Crop region selected
	 * @param rect normalized selection region (values in range [0.0..1.0])
	 */
	void cropped(const QRectF &rect);

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	QPixmap m_pixmap;
	QRubberBand *m_rubberband;
	QPoint m_cropStart;
	QRect m_targetRect;
	bool m_upscale;
};

#endif // FLIPBOOKVIEW_H
