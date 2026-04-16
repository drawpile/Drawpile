// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/brush_preview.h>
#include <dpengine/image.h>
#include <dpmsg/blend_mode.h>
}
#include "libclient/drawdance/brushpreview.h"
#include "libclient/drawdance/global.h"
#include "libclient/drawdance/image.h"
#include <QPainter>
#include <QPalette>

namespace drawdance {

BrushPreview::BrushPreview()
	: m_data(DP_brush_preview_new())
{
}

BrushPreview::~BrushPreview()
{
	DP_brush_preview_free(m_data);
}

void BrushPreview::setPalette(const QPalette &pal)
{
	QColor foreground = pal.color(QPalette::Active, QPalette::Text);
	QColor background = pal.color(QPalette::Active, QPalette::Base);
	QColor smudge = pal.color(QPalette::Active, QPalette::Highlight);

	// Some themes make these the same or very similar.
	int smudgeR = smudge.red();
	int smudgeG = smudge.green();
	int smudgeB = smudge.blue();
	int diff = DP_square_int(foreground.red() - smudgeR) +
			   DP_square_int(foreground.green() - smudgeG) +
			   DP_square_int(foreground.blue() - smudgeB);
	if(diff < 14000) {
		if(smudgeR == smudgeG && smudgeG == smudgeB) {
			smudge = QColor::fromHsv(
				(background.hue() + 128) % 255, (smudge.saturation() + 255) / 2,
				(smudge.value() + 255) / 2);
		} else {
			smudge = QColor::fromHsv(
				background.hue(), (smudge.saturation() + 255) / 2,
				(smudge.value() + 255) / 2);
		}
	}

	DP_brush_preview_palette_set(
		m_data, foreground.rgb(), background.rgb(), smudge.rgb());
}

void BrushPreview::reset(QSize size)
{
	if(size != m_pixmap.size()) {
		m_pixmap = QPixmap(size);
	}
}

void BrushPreview::setSizeLimit(int limit)
{
	DP_brush_preview_size_limit_set(m_data, limit);
}

void BrushPreview::renderClassic(
	const DP_ClassicBrush &brush, DP_BrushPreviewStyle style,
	DP_BrushPreviewShape shape)
{
	QSize size = m_pixmap.size();
	DrawContext dc = DrawContextPool::acquire();
	DP_brush_preview_render_classic(
		m_data, dc.get(), size.width(), size.height(), &brush, style, shape);
}

void BrushPreview::renderMyPaint(
	const DP_MyPaintBrush &brush, const DP_MyPaintSettings &settings,
	DP_BrushPreviewStyle style, DP_BrushPreviewShape shape)
{
	QSize size = m_pixmap.size();
	DrawContext dc = DrawContextPool::acquire();
	DP_brush_preview_render_mypaint(
		m_data, dc.get(), size.width(), size.height(), &brush, &settings, style,
		shape);
}

void BrushPreview::paint(const QPixmap &background)
{
	QImage img = wrapImage(DP_brush_preview_to_image(m_data));
	m_pixmap.convertFromImage(img);
	if(!background.isNull()) {
		QPainter painter(&m_pixmap);
		painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
		painter.drawTiledPixmap(m_pixmap.rect(), background);
	}
}

QPixmap BrushPreview::classicBrushPreviewDab(
	const DP_ClassicBrush &cb, int width, int height, const QColor &color)
{
	DrawContext dc = DrawContextPool::acquire();
	QImage img = wrapImage(DP_classic_brush_preview_dab(
		&cb, dc.get(), width, height, color.rgba()));
	return QPixmap::fromImage(img);
}

}
