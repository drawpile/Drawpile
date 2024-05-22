// SPDX-License-Identifier: GPL-3.0-only
#ifndef DESKTOP_UTILS_FUSIONUI_H
#define DESKTOP_UTILS_FUSIONUI_H
#include <QColor>
#include <QLinearGradient>
#include <QProxyStyle>

namespace fusionui {

bool looksLikeFusionStyle(const QString &themeStyle);

class FusionProxyStyle : public QProxyStyle {
public:
	explicit FusionProxyStyle(const QString &key);

	void drawPrimitive(
		PrimitiveElement element, const QStyleOption *option, QPainter *painter,
		const QWidget *widget = nullptr) const override;

	QIcon standardIcon(
		StandardPixmap standardIcon, const QStyleOption *opt = nullptr,
		const QWidget *widget = nullptr) const override;

private:
	static QColor
	mergedColors(const QColor &colorA, const QColor &colorB, int factor = 50);

	static bool isMacSystemPalette(const QPalette &pal);
	static QColor highlight(const QPalette &pal);
	static QColor outline(const QPalette &pal);
	static QColor highlightedOutline(const QPalette &pal);
	static QColor innerContrastLine();
	static QColor buttonColor(const QPalette &pal);

	static QLinearGradient
	qt_fusion_gradient(const QRect &rect, const QBrush &baseColor);
};

}

#endif
