// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_BRUSHPREVIEW_H
#define DESKTOP_WIDGETS_BRUSHPREVIEW_H
#include "libclient/brushes/brush.h"
#include "libclient/drawdance/brushpreview.h"
#include "libclient/utils/debouncetimer.h"
#include <QFrame>

namespace widgets {

class BrushPreview final : public QFrame {
	Q_OBJECT
public:
	explicit BrushPreview(
		QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

	void setPreviewShape(DP_BrushPreviewShape shape);
	DP_BrushPreviewShape previewShape() const { return m_shape; }

	const brushes::ActiveBrush &brush() const { return m_brush; }

	void setBrush(const brushes::ActiveBrush &brush);
	void setBrushSizeLimit(int brushSizeLimit);

	void clearPreset();
	void setPreset(const QPixmap &thumbnail, bool changed);
	void setPresetThumbnail(const QPixmap &thumbnail);
	void setPresetChanged(bool changed);

signals:
	void requestEditor();

protected:
	void resizeEvent(QResizeEvent *) override;
	void changeEvent(QEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

private:
	void triggerPreviewUpdate();

	void updateBackground();
	void updatePreview(qreal dpr);
	void updatePreset(qreal dpr);

	QRect previewRect() const;
	QRect presetRect() const;
	QRect changeIconRect() const;

	QPixmap m_background;
	brushes::ActiveBrush m_brush;
	drawdance::BrushPreview m_brushPreview;
	DP_BrushPreviewShape m_shape = DP_BRUSH_PREVIEW_STROKE;
	QPixmap m_presetThumbnail;
	QPixmap m_presetCache;
	QPixmap m_changeIconCache;
	bool m_presetChanged = false;
	bool m_presetEnabled = false;
	bool m_needUpdate = false;
	int m_lastDpr = 1.0;
	DebounceTimer m_debounce;
};

}

#endif
