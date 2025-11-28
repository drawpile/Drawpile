// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_VIEW_COLORPICKRENDERER
#define LIBCLIENT_VIEW_COLORPICKRENDERER
#include <QColor>
#include <QPainterPath>
#include <QPixmap>

class QPainter;

namespace view {

class ColorPickRenderer {
public:
	static constexpr int SIZE = 200;

	ColorPickRenderer(const QColor &color, const QColor &comparisonColor);

	bool updateColor(const QColor &color);
	bool updateComparisonColor(const QColor &comparisonColor);

	void paint(QPainter *painter, const QRect &rect, const QColor &baseColor);

	static bool shouldShow(int source, int visibility);
	static int defaultVisibility();

private:
	const QPainterPath &getPath(qreal penWidth, const QRectF &cacheRect);

	static QColor sanitizeColor(const QColor &color);

	QColor m_color;
	QColor m_comparisonColor;
	QPixmap m_cache;
	QRectF m_pathCacheRect;
	qreal m_pathPenWidth = 0.0;
	QPainterPath m_path;
};

}

#endif
