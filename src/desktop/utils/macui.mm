// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/utils/macui.h"

#include <QColor>
#include <QComboBox>
#include <QDebug>
#include <QLinearGradient>
#include <QPainter>
#include <QPalette>
#include <QProxyStyle>
#include <QStyle>
#include <QStyleOption>
#include <QStyleOptionButton>

#import <Cocoa/Cocoa.h>

class QWidget;

namespace macui {

void MacProxyStyle::drawPrimitive(QStyle::PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
	if (element != QStyle::PE_PanelStatusBar) {
		return QProxyStyle::drawPrimitive(element, option, painter, widget);
	}

	// The "native" style status bar looks weird because it uses the same
	// gradient as the title bar but is taller than a normal status bar. This
	// makes it look better.
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

// This is cut'n'paste from private Qt style helper code
enum WidgetSizePolicy { SizeLarge = 0, SizeSmall = 1, SizeMini = 2, SizeDefault = -1 };
WidgetSizePolicy widgetSizePolicy(const QWidget *widget, const QStyleOption *opt)
{
	while (widget) {
		if (widget->testAttribute(Qt::WA_MacMiniSize)) {
			return SizeMini;
		} else if (widget->testAttribute(Qt::WA_MacSmallSize)) {
			return SizeSmall;
		} else if (widget->testAttribute(Qt::WA_MacNormalSize)) {
			return SizeLarge;
		}
		widget = widget->parentWidget();
	}

	if (opt && opt->state & QStyle::State_Mini)
		return SizeMini;
	else if (opt && opt->state & QStyle::State_Small)
		return SizeSmall;

	return SizeDefault;
}

int MacProxyStyle::layoutSpacing(
	QSizePolicy::ControlType control1, QSizePolicy::ControlType control2,
	Qt::Orientation orientation, const QStyleOption *option, const QWidget *widget
) const
{
	if (widgetSizePolicy(widget, option) == SizeDefault) {
		// QMacStyle has very busted ideas about how far apart some controls should
		// be. This does not handle the small/mini controls, but Drawpile does not
		// use any of those.
		if (orientation == Qt::Vertical) {
			if (control2 == QSizePolicy::ComboBox) {
				switch (control1) {
				// Based on having eyeballs
				case QSizePolicy::Line: return 8;
				// Based on having eyeballs
				case QSizePolicy::CheckBox: return 5;
				// Based on the General settings in System Preferences
				default: return 4;
				}
			} else if (control1 & control2 & QSizePolicy::CheckBox) {
				// Based on having eyeballs
				return 3;
			}
		} else {
			// This reverses the QStyle::SE_ComboBoxLayoutItem change to make
			// sure things do not get too close in non-grid layouts
			if ((control1 | control2) & QSizePolicy::ComboBox) {
				auto spacing = QProxyStyle::layoutSpacing(control1, control2, orientation, option, widget);
				if (control1 == QSizePolicy::ComboBox) {
					spacing += 3;
				}
				if (control2 == QSizePolicy::ComboBox) {
					spacing += 2;
				}
				return spacing;
			}
		}
	}

	return QProxyStyle::layoutSpacing(control1, control2, orientation, option, widget);
}

QRect MacProxyStyle::subElementRect(QStyle::SubElement element, const QStyleOption *option, const QWidget *widget) const
{
	auto rect = QProxyStyle::subElementRect(element, option, widget);
	if (widgetSizePolicy(widget, option) == SizeDefault) {
		switch (element) {
		case QStyle::SE_CheckBoxLayoutItem: // QTBUG-14643, QTBUG-85142
			rect = option->rect;
			// +2 ensures that the checkbox has correct visual padding to line up
			// with e.g. radio buttons which are slightly wider
			rect.setLeft(option->rect.left() + 2);
			rect.setRight(option->rect.right());
			break;
		case QStyle::SE_ComboBoxLayoutItem:
			// This makes the layout geometry closer to the visual edge of the combo
			// box so that there is no ugly extra spacing between rows containing
			// combo boxes and rows containing other controls
			rect.adjust(2, 0, -3, 0);
			break;
		default: {}
		}
	}
	return rect;
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
