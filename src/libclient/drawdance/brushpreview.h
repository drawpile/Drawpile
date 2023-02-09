#ifndef DRAWDANCE_BRUSH_PREVIEW_H
#define DRAWDANCE_BRUSH_PREVIEW_H

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

    const QPixmap &pixmap() const { return m_pixmap; }

    void reset(QSize size);

    void renderClassic(const DP_ClassicBrush &brush, DP_BrushPreviewShape shape);

    void renderMyPaint(
        const DP_MyPaintBrush &brush, const DP_MyPaintSettings &settings,
        DP_BrushPreviewShape shape);

    void floodFill(
        const QColor &color, float tolerance, int expansion, int featherRadius, bool under);

    void paint(const QPixmap &background);

    static QPixmap classicBrushPreviewDab(
        const DP_ClassicBrush &cb, int width, int height, const QColor &color);

private:
    DP_BrushPreview *m_data;
    QPixmap m_pixmap;
};

}


#endif
