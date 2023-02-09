extern "C" {
#include <dpengine/brush_preview.h>
#include <dpengine/image.h>
#include <dpmsg/blend_mode.h>
}

#include "brushpreview.h"
#include "drawcontextpool.h"
#include "image.h"
#include <QPainter>

namespace drawdance {

BrushPreview::BrushPreview()
    : m_data{DP_brush_preview_new()}
    , m_pixmap{}
{
}

BrushPreview::~BrushPreview()
{
    DP_brush_preview_free(m_data);
}

void BrushPreview::reset(QSize size)
{
    if (size != m_pixmap.size()) {
        m_pixmap = QPixmap{size};
    }
}

void BrushPreview::renderClassic(const DP_ClassicBrush &brush, DP_BrushPreviewShape shape)
{
    QSize size = m_pixmap.size();
    DrawContext dc = DrawContextPool::acquire();
    DP_brush_preview_render_classic(
        m_data, dc.get(), size.width(), size.height(), &brush, shape);
}

void BrushPreview::renderMyPaint(
    const DP_MyPaintBrush &brush, const DP_MyPaintSettings &settings,
    DP_BrushPreviewShape shape)
{
    QSize size = m_pixmap.size();
    DrawContext dc = DrawContextPool::acquire();
    DP_brush_preview_render_mypaint(
        m_data, dc.get(), size.width(), size.height(), &brush, &settings, shape);
}

void BrushPreview::floodFill(
    const QColor &color, float tolerance, int expansion, int featherRadius, bool under)
{
    int blendMode;
    QColor effectiveColor;
    if(color.alpha() == 0) {
        blendMode = DP_BLEND_MODE_ERASE;
        effectiveColor = Qt::black;
    } else if(under) {
        blendMode = DP_BLEND_MODE_BEHIND;
        effectiveColor = color;
    } else {
        blendMode = DP_BLEND_MODE_NORMAL;
        effectiveColor = color;
    }
    DP_brush_preview_render_flood_fill(
        m_data, DP_upixel_float_from_color(effectiveColor.rgba()), tolerance,
        expansion, featherRadius, blendMode);
}

void BrushPreview::paint(const QPixmap &background)
{
    QImage img = wrapImage(DP_brush_preview_to_image(m_data));
    m_pixmap.convertFromImage(img);

    QPainter painter{&m_pixmap};
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
    painter.drawTiledPixmap(m_pixmap.rect(), background);
}

QPixmap BrushPreview::classicBrushPreviewDab(
    const DP_ClassicBrush &cb, int width, int height, const QColor &color)
{
    DrawContext dc = DrawContextPool::acquire();
    QImage img = wrapImage(DP_classic_brush_preview_dab(&cb, dc.get(), width, height, color.rgba()));
    return QPixmap::fromImage(img);
}

}
