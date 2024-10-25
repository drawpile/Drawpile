// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_RESIZERWIDGET_H
#define DESKTOP_WIDGETS_RESIZERWIDGET_H
#include <QWidget>

namespace widgets {

class ResizerWidget final : public QWidget {
	Q_OBJECT
public:
	explicit ResizerWidget(QWidget *parent = nullptr);

	void setBackgroundColor(const QColor &bgColor);
	void setImage(const QImage &image);
	void setTargetSize(const QSize &size);
	void setOriginalSize(const QSize &size);

	QSize targetSize() const { return m_targetSize; }
	QSize originalSize() const { return m_originalSize; }

	QPoint offset() const { return m_offset; }
	void setOffset(const QPoint &offset);
	void changeOffsetBy(const QPoint &change) { setOffset(m_offset + change); }

public slots:
	void center();

signals:
	void offsetChanged(const QPoint &);

protected:
	void paintEvent(QPaintEvent *) override;
	void resizeEvent(QResizeEvent *) override;
	void mousePressEvent(QMouseEvent *) override;
	void mouseMoveEvent(QMouseEvent *) override;

private:
	void updateScales();

	QSize m_originalSize;
	QSize m_targetSize;
	QPoint m_offset;

	QRectF m_targetScaled;
	QSize m_originalScaled;
	qreal m_scale;

	QPoint m_grabPoint;
	QPoint m_grabOffset;

	QColor m_bgColor;
	QPixmap m_originalPixmap;
};

}

#endif
