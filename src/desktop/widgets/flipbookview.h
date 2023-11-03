// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef FLIPBOOKVIEW_H
#define FLIPBOOKVIEW_H
#include <QWidget>

class QRubberBand;

class FlipbookView final : public QWidget {
	Q_OBJECT
public:
	explicit FlipbookView(QWidget *parent = nullptr);

	bool isUpscaling() const { return m_upscale; }

	void setLoading(bool loading);
	bool isLoading() const { return m_loading; }

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
	void timerEvent(QTimerEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	static constexpr int SPINNER_DOTS = 8;

	QPixmap m_pixmap;
	QRubberBand *m_rubberband = nullptr;
	QPoint m_cropStart;
	QRect m_targetRect;
	bool m_upscale = false;
	bool m_loading = false;
	int m_spinnerTimer = 0;
	int m_currentSpinnerDot = 0;
};

#endif // FLIPBOOKVIEW_H
