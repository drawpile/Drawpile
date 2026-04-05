// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_DRAWDANCE_BRUSH_PREVIEW_H
#define LIBCLIENT_DRAWDANCE_BRUSH_PREVIEW_H
extern "C" {
#include <dpengine/brush_preview.h>
}
#include <QPixmap>
#include <QSize>

namespace drawdance {

class BrushPreview final {
public:
	BrushPreview();
	~BrushPreview();

	BrushPreview(const BrushPreview &) = delete;
	BrushPreview(BrushPreview &&) = delete;
	BrushPreview &operator=(const BrushPreview &) = delete;
	BrushPreview &operator=(BrushPreview &&) = delete;

	void setPalette(
		const QColor &foreground, const QColor &background,
		const QColor &smudge);

	const QPixmap &pixmap() const { return m_pixmap; }

	void reset(QSize size);

	void setSizeLimit(int limit);

	void renderClassic(
		const DP_ClassicBrush &brush, DP_BrushPreviewStyle style,
		DP_BrushPreviewShape shape);

	void renderMyPaint(
		const DP_MyPaintBrush &brush, const DP_MyPaintSettings &settings,
		DP_BrushPreviewStyle style, DP_BrushPreviewShape shape);

	void paint(const QPixmap &background = QPixmap());

	static QPixmap classicBrushPreviewDab(
		const DP_ClassicBrush &cb, int width, int height, const QColor &color);

private:
	DP_BrushPreview *m_data;
	QPixmap m_pixmap;
};

}


#endif
