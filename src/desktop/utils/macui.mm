// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/utils/macui.h"

#include <QApplication>
#include <QColor>
#include <QLinearGradient>
#include <QPainter>
#include <QPalette>
#include <QProxyStyle>
#include <QStyle>
#include <QStyleOption>
#include <QWidget>

#import <Cocoa/Cocoa.h>

namespace macui {

// The "native" style status bar looks weird because it uses the same gradient
// as the title bar but is taller than a normal status bar. This makes it look
// better.
void MacViewStatusBarProxyStyle::drawPrimitive(QStyle::PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
	if (element != QStyle::PE_PanelStatusBar) {
		return QProxyStyle::drawPrimitive(element, option, painter, widget);
	}

	static const QColor darkLine(0, 0, 0);
	static const QColor darkFill(48, 48, 48);
	static const QLinearGradient darkGradient = [](){
		QLinearGradient gradient;
		gradient.setColorAt(0, QColor(67, 67, 67));
		gradient.setColorAt(1, QColor(48, 48, 48));
		return gradient;
	}();
	static const QColor lightLine(193, 193, 193);
	static const QColor lightFill(246, 246, 246);
	static const QLinearGradient lightGradient = [](){
		QLinearGradient gradient;
		gradient.setColorAt(0, QColor(240, 240, 240));
		gradient.setColorAt(1, QColor(224, 224, 224));
		return gradient;
	}();

	const bool dark = QPalette().color(QPalette::Window).lightness() < 128;

	if(option->state & QStyle::State_Active) {
		auto linearGrad = dark ? darkGradient : lightGradient;
		linearGrad.setStart(0, option->rect.top());
		linearGrad.setFinalStop(0, option->rect.bottom());
		painter->fillRect(option->rect, linearGrad);
	} else {
		painter->fillRect(option->rect, dark ? darkFill : lightFill);
	}

	painter->setPen(dark ? darkLine : lightLine);
	painter->drawLine(option->rect.left(), option->rect.top(), option->rect.right(), option->rect.top());
}

bool setNativeAppearance(Appearance appearance)
{
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_14
	if (@available(macOS 10.14, *)) {
		NSAppearance *nsAppearance = nil;
		switch (appearance) {
		case Appearance::Light: nsAppearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua]; break;
		case Appearance::Dark: nsAppearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]; break;
		case Appearance::System: break;
		}

		[[NSApplication sharedApplication] setAppearance:nsAppearance];
		return true;
	}
#endif

	return false;
}

} // namespace macui
