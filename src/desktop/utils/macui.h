// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_UTILS_MACUI_H
#define DESKTOP_UTILS_MACUI_H

#include <QProxyStyle>
#include <QSizePolicy>
#include <QStyle>
#include <Qt>

class QPainter;
class QStyleOption;
class QWidget;

namespace macui {

enum class Appearance {
	System,
	Light,
	Dark
};

class MacProxyStyle final : public QProxyStyle {
	void drawPrimitive(QStyle::PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = nullptr) const override;
	int layoutSpacing(
		QSizePolicy::ControlType control1, QSizePolicy::ControlType control2,
		Qt::Orientation orientation, const QStyleOption *option, const QWidget *widget
	) const override;
	QRect subElementRect(QStyle::SubElement element, const QStyleOption *option, const QWidget *widget) const override;
};

bool setNativeAppearance(Appearance appearance);

} // namespace macui

#endif
