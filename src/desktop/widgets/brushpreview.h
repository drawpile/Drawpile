// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_BRUSHPREVIEW_H
#define DESKTOP_WIDGETS_BRUSHPREVIEW_H
#include "libclient/brushes/brush.h"
#include "libclient/drawdance/brushpreview.h"
#include <QFrame>

namespace widgets {

class BrushPreview final : public QFrame {
	Q_OBJECT
public:
	explicit BrushPreview(
		QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

	void setPreviewShape(DP_BrushPreviewShape shape);
	DP_BrushPreviewShape previewShape() const { return m_shape; }

	QColor brushColor() const { return m_brush.qColor(); }
	const brushes::ActiveBrush &brush() const { return m_brush; }

public slots:
	void setBrush(const brushes::ActiveBrush &brush);

signals:
	void requestColorChange();

protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *) override;
	void changeEvent(QEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *) override;

private:
	void updatePreview(qreal dpr);
	void updateBackground();

	QPixmap m_background;
	brushes::ActiveBrush m_brush;
	drawdance::BrushPreview m_brushPreview;
	DP_BrushPreviewShape m_shape = DP_BRUSH_PREVIEW_STROKE;
	bool m_needUpdate = false;
	int m_lastDpr = 1.0;
};

}

#endif
