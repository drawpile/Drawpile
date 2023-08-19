// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_UTILS_FUSIONUI_H
#define DESKTOP_UTILS_FUSIONUI_H
#include <QColor>
#include <QProxyStyle>

namespace fusionui {

class FusionProxyStyle : public QProxyStyle {
public:
	explicit FusionProxyStyle(QStyle *style);

	void drawPrimitive(
		PrimitiveElement element, const QStyleOption *option, QPainter *painter,
		const QWidget *widget = nullptr) const override;

private:
	static QColor
	mergedColors(const QColor &colorA, const QColor &colorB, int factor = 50);

	static bool isMacSystemPalette(const QPalette &pal);
	static QColor highlight(const QPalette &pal);
	static QColor outline(const QPalette &pal);
	static QColor highlightedOutline(const QPalette &pal);
};

}

#endif
