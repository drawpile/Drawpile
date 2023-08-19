// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/utils/fusionui.h"
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <QtWidgets/private/qstylehelper_p.h>

#ifdef Q_OS_MACOS
QT_WARNING_PUSH
QT_WARNING_DISABLE_CLANG("-Wc++98-compat-extra-semi")
QT_WARNING_DISABLE_CLANG("-Wextra-semi")
QT_WARNING_DISABLE_CLANG("-Wgnu-anonymous-struct")
QT_WARNING_DISABLE_CLANG("-Wnested-anon-types")
QT_WARNING_DISABLE_CLANG("-Wshadow")
QT_WARNING_DISABLE_CLANG("-Wsuggest-destructor-override")
QT_WARNING_DISABLE_CLANG("-Wunused-template")
QT_WARNING_DISABLE_CLANG("-Wzero-as-null-pointer-constant")
#	include <QtGui/private/qguiapplication_p.h>
#	include <qpa/qplatformtheme.h>
QT_WARNING_POP
#endif

namespace fusionui {

FusionProxyStyle::FusionProxyStyle(QStyle *style)
	: QProxyStyle{style}
{
}

void FusionProxyStyle::drawPrimitive(
	PrimitiveElement element, const QStyleOption *option, QPainter *painter,
	const QWidget *widget) const
{
	if(element == PrimitiveElement::PE_IndicatorCheckBox) {
		// SPDX-SnippetBegin
		// SPDX-License-Identifier: GPL-3.0-or-later
		// SDPX—SnippetName: checkbox painting from qfusionstyle.cpp
		QRect rect = option->rect;
		int state = option->state;
		painter->save();
		if(const QStyleOptionButton *checkbox =
			   qstyleoption_cast<const QStyleOptionButton *>(option)) {
			painter->setRenderHint(QPainter::Antialiasing, true);
			painter->translate(0.5, 0.5);
			rect = rect.adjusted(0, 0, -1, -1);

			QColor pressedColor = mergedColors(
				option->palette.base().color(),
				option->palette.windowText().color(), 85);
			painter->setBrush(Qt::NoBrush);

			// Gradient fill
			QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
			gradient.setColorAt(
				0, (state & State_Sunken)
					   ? pressedColor
					   : option->palette.base().color().darker(115));
			gradient.setColorAt(
				0.15, (state & State_Sunken) ? pressedColor
											 : option->palette.base().color());
			gradient.setColorAt(
				1, (state & State_Sunken) ? pressedColor
										  : option->palette.base().color());

			painter->setBrush(
				(state & State_Sunken) ? QBrush(pressedColor) : gradient);

			// Drawpile patch. This is the only (!) thing we want to change, but
			// we have to pull in all this other garbage to actually do it.
			// Lightening the outline by only 110 ends up being completely
			// invisible in dark themes, so we increase that some more.
			bool dark =
				option->palette.color(QPalette::Window).lightness() < 128;
			painter->setPen(
				QPen(outline(option->palette).lighter(dark ? 170 : 110)));
			// End of Drawpile patch.

			if(option->state & State_HasFocus &&
			   option->state & State_KeyboardFocusChange)
				painter->setPen(QPen(highlightedOutline(option->palette)));
			painter->drawRect(rect);

			QColor checkMarkColor = option->palette.text().color().darker(120);
			const qreal checkMarkPadding =
				1 + rect.width() * 0.13; // at least one pixel padding

			if(checkbox->state & State_NoChange) {
				gradient = QLinearGradient(rect.topLeft(), rect.bottomLeft());
				checkMarkColor.setAlpha(80);
				gradient.setColorAt(0, checkMarkColor);
				checkMarkColor.setAlpha(140);
				gradient.setColorAt(1, checkMarkColor);
				checkMarkColor.setAlpha(180);
				painter->setPen(QPen(checkMarkColor, 1));
				painter->setBrush(gradient);
				painter->drawRect(rect.adjusted(
					checkMarkPadding, checkMarkPadding, -checkMarkPadding,
					-checkMarkPadding));

			} else if(checkbox->state & State_On) {
				const qreal dpi = QStyleHelper::dpi(option);
				qreal penWidth = QStyleHelper::dpiScaled(1.5, dpi);
				penWidth = qMax<qreal>(penWidth, 0.13 * rect.height());
				penWidth = qMin<qreal>(penWidth, 0.20 * rect.height());
				QPen checkPen = QPen(checkMarkColor, penWidth);
				checkMarkColor.setAlpha(210);
				painter->translate(
					QStyleHelper::dpiScaled(-0.8, dpi),
					QStyleHelper::dpiScaled(0.5, dpi));
				painter->setPen(checkPen);
				painter->setBrush(Qt::NoBrush);

				// Draw checkmark
				QPainterPath path;
				const qreal rectHeight =
					rect.height(); // assuming height equals width
				path.moveTo(
					checkMarkPadding + rectHeight * 0.11, rectHeight * 0.47);
				path.lineTo(rectHeight * 0.5, rectHeight - checkMarkPadding);
				path.lineTo(rectHeight - checkMarkPadding, checkMarkPadding);
				painter->drawPath(path.translated(rect.topLeft()));
			}
		}
		painter->restore();
		// SPDX-SnippetEnd
	} else {
		QProxyStyle::drawPrimitive(element, option, painter, widget);
	}
}

// SPDX-SnippetBegin
// SPDX-License-Identifier: GPL-3.0-or-later
// SDPX—SnippetName: ancillary qfusionstyle.cpp not available externally

QColor FusionProxyStyle::mergedColors(
	const QColor &colorA, const QColor &colorB, int factor)
{
	const int maxFactor = 100;
	QColor tmp = colorA;
	tmp.setRed(
		(tmp.red() * factor) / maxFactor +
		(colorB.red() * (maxFactor - factor)) / maxFactor);
	tmp.setGreen(
		(tmp.green() * factor) / maxFactor +
		(colorB.green() * (maxFactor - factor)) / maxFactor);
	tmp.setBlue(
		(tmp.blue() * factor) / maxFactor +
		(colorB.blue() * (maxFactor - factor)) / maxFactor);
	return tmp;
}

// SPDX-SnippetEnd

// SPDX-SnippetBegin
// SPDX-License-Identifier: GPL-3.0-or-later
// SDPX—SnippetName: ancillary qfusionstyle_p_p.h stuff not available outside

bool FusionProxyStyle::isMacSystemPalette(const QPalette &pal)
{
	Q_UNUSED(pal);
#if defined(Q_OS_MACOS)
	const QPalette *themePalette =
		QGuiApplicationPrivate::platformTheme()->palette();
	if(themePalette &&
	   themePalette->color(QPalette::Normal, QPalette::Highlight) ==
		   pal.color(QPalette::Normal, QPalette::Highlight) &&
	   themePalette->color(QPalette::Normal, QPalette::HighlightedText) ==
		   pal.color(QPalette::Normal, QPalette::HighlightedText))
		return true;
#endif
	return false;
}

QColor FusionProxyStyle::highlight(const QPalette &pal)
{
	if(isMacSystemPalette(pal))
		return QColor(60, 140, 230);
	return pal.color(QPalette::Highlight);
}

QColor FusionProxyStyle::outline(const QPalette &pal)
{
	if(pal.window().style() == Qt::TexturePattern)
		return QColor(0, 0, 0, 160);
	return pal.window().color().darker(140);
}

QColor FusionProxyStyle::highlightedOutline(const QPalette &pal)
{
	QColor highlightedOutline = highlight(pal).darker(125);
	if(highlightedOutline.value() > 160)
		highlightedOutline.setHsl(
			highlightedOutline.hue(), highlightedOutline.saturation(), 160);
	return highlightedOutline;
}

// SPDX-SnippetEnd

}
