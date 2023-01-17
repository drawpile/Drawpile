// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_UTILS_MACUI_H
#define DESKTOP_UTILS_MACUI_H

#include <QProxyStyle>
#include <QStyle>

class QPainter;
class QStyleOption;
class QWidget;

namespace macui {

enum class Appearance {
	System,
	Light,
	Dark
};

class MacViewStatusBarProxyStyle : public QProxyStyle {
	void drawPrimitive(QStyle::PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = nullptr) const override;
};

bool setNativeAppearance(Appearance appearance);

} // namespace macui

#endif
