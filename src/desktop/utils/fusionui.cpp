// SPDX-License-Identifier: GPL-3.0-only
// This whole thing is terrible hackery and duplication of code from the
// internal Qt styling code because it's not exposed or exported otherwise. This
// also breaks horribly on some Windows locales because I guess the Fusion style
// has special hacks in there that don't apply if you proxy it. In our own
// releases, we just patch Qt directly and avoid all this nonsense, but e.g.
// Flatpak or other Linux package managers don't have that option available.
#include "desktop/utils/fusionui.h"
#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QStyleFactory>
#include <QStyleOption>
#include <QtWidgets/private/qstylehelper_p.h>
QT_WARNING_PUSH
QT_WARNING_DISABLE_CLANG("-Wextra-semi")
QT_WARNING_DISABLE_GCC("-Wextra-semi")
QT_WARNING_DISABLE_CLANG("-Wold-style-cast")
QT_WARNING_DISABLE_GCC("-Wold-style-cast")
QT_WARNING_DISABLE_CLANG("-Wpedantic")
QT_WARNING_DISABLE_GCC("-Wpedantic")
QT_WARNING_DISABLE_CLANG("-Wshadow")
QT_WARNING_DISABLE_GCC("-Wshadow")
QT_WARNING_DISABLE_CLANG("-Wzero-as-null-pointer-constant")
QT_WARNING_DISABLE_GCC("-Wzero-as-null-pointer-constant")
#include <QtWidgets/private/qapplication_p.h>
#ifdef Q_OS_MACOS
QT_WARNING_DISABLE_CLANG("-Wc++98-compat-extra-semi")
QT_WARNING_DISABLE_CLANG("-Wgnu-anonymous-struct")
QT_WARNING_DISABLE_CLANG("-Wnested-anon-types")
QT_WARNING_DISABLE_CLANG("-Wsuggest-destructor-override")
QT_WARNING_DISABLE_CLANG("-Wunused-template")
QT_WARNING_DISABLE_CLANG("-Wzero-as-null-pointer-constant")
#	include <QtGui/private/qguiapplication_p.h>
#	include <qpa/qplatformtheme.h>
#endif
QT_WARNING_POP

namespace fusionui {

bool looksLikeFusionStyle(const QString &themeStyle)
{
	return themeStyle.startsWith(
			   QStringLiteral("fusion"), Qt::CaseInsensitive) ||
		   (themeStyle.isEmpty() &&
			QApplicationPrivate::desktopStyleKey().startsWith(
				QStringLiteral("fusion"), Qt::CaseInsensitive));
}

FusionProxyStyle::FusionProxyStyle(const QString &key)
	: QProxyStyle(key)
{
}

void FusionProxyStyle::drawPrimitive(
	PrimitiveElement element, const QStyleOption *option, QPainter *painter,
	const QWidget *widget) const
{
	if(element == PrimitiveElement::PE_IndicatorCheckBox) {
		// SPDX-SnippetBegin
		// SPDX-License-Identifier: GPL-3.0-only
		// SDPX—SnippetName: checkbox painting from qfusionstyle.cpp
		QRect rect = option->rect;
		int state = option->state;
		QColor outline = FusionProxyStyle::outline(option->palette);
		QColor highlightedOutline =
			FusionProxyStyle::highlightedOutline(option->palette);

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
			// Drawpile patch: improve checkbox outline contrast in dark themes.
			bool dark =
				option->palette.color(QPalette::Window).lightness() < 128;
			painter->setPen(QPen(outline.lighter(dark ? 170 : 110)));
			// End Drawpile patch.

			if(option->state & State_HasFocus &&
			   option->state & State_KeyboardFocusChange)
				painter->setPen(QPen(highlightedOutline));
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

	} else if(
		element == PE_PanelButtonCommand &&
		((option->state & State_Sunken) || (option->state & State_On))) {
		// SPDX-SnippetBegin
		// SPDX-License-Identifier: GPL-3.0-only
		// SDPX—SnippetName: panel button command painting from qfusionstyle.cpp
		QColor outline = FusionProxyStyle::outline(option->palette);
		QColor highlightedOutline =
			FusionProxyStyle::highlightedOutline(option->palette);

		bool isDefault = false;
		QRect r;

		if(const QStyleOptionButton *button =
			   qstyleoption_cast<const QStyleOptionButton *>(option)) {
			isDefault =
				(button->features & QStyleOptionButton::DefaultButton) &&
				(button->state & State_Enabled);
		}

		bool isEnabled = option->state & State_Enabled;
		bool hasFocus =
			(option->state & State_HasFocus &&
			 option->state & State_KeyboardFocusChange);
		QColor buttonColor = FusionProxyStyle::buttonColor(option->palette);

		QColor darkOutline = outline;
		if(hasFocus | isDefault) {
			darkOutline = highlightedOutline;
		}

		if(isDefault)
			buttonColor =
				mergedColors(buttonColor, highlightedOutline.lighter(130), 90);

		QRect rect = option->rect;
		QPainter *p = painter;
		r = rect.adjusted(0, 1, -1, 0);

		p->setRenderHint(QPainter::Antialiasing, true);
		p->translate(0.5, -0.5);

		QLinearGradient gradient = FusionProxyStyle::qt_fusion_gradient(
			rect, (isEnabled && option->state & State_MouseOver)
					  ? buttonColor
					  : buttonColor.darker(104));
		p->setPen(Qt::transparent);
		// Drawpile patch: improve depressed button contrast in dark themes.
		int lightness = option->palette.color(QPalette::Window).lightness();
		p->setBrush(QBrush(buttonColor.darker(
			lightness > 127	 ? 110
			: lightness > 63 ? 140
							 : 180)));
		// End Drawpile patch.
		p->drawRoundedRect(r, 2.0, 2.0);
		p->setBrush(Qt::NoBrush);

		// Outline
		p->setPen(
			!isEnabled ? QPen(darkOutline.lighter(115)) : QPen(darkOutline));
		p->drawRoundedRect(r, 2.0, 2.0);

		p->setPen(FusionProxyStyle::innerContrastLine());
		p->drawRoundedRect(r.adjusted(1, 1, -1, -1), 2.0, 2.0);
		// SPDX-SnippetEnd

	} else {
		QProxyStyle::drawPrimitive(element, option, painter, widget);
	}
}

QIcon FusionProxyStyle::standardIcon(
	StandardPixmap standardIcon, const QStyleOption *opt,
	const QWidget *widget) const
{
	switch(standardIcon) {
	case SP_ToolBarHorizontalExtensionButton:
		if((opt && opt->direction == Qt::RightToLeft) ||
		   (!opt && QGuiApplication::isRightToLeft())) {
			return QIcon::fromTheme(
				QStringLiteral("toolbar-ext-h-rtl-drawpile"));
		} else {
			return QIcon::fromTheme(QStringLiteral("toolbar-ext-h-drawpile"));
		}
	case SP_ToolBarVerticalExtensionButton:
		return QIcon::fromTheme(QStringLiteral("toolbar-ext-v-drawpile"));
	default:
		return QProxyStyle::standardIcon(standardIcon, opt, widget);
	}
}

// SPDX-SnippetBegin
// SPDX-License-Identifier: GPL-3.0-only
// SDPX—SnippetName: private Fusion style functions not exposed externally

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

QColor FusionProxyStyle::innerContrastLine()
{
	return QColor(255, 255, 255, 30);
}

QColor FusionProxyStyle::buttonColor(const QPalette &pal)
{
	QColor buttonColor = pal.button().color();
	int val = qGray(buttonColor.rgb());
	buttonColor = buttonColor.lighter(100 + qMax(1, (180 - val) / 6));
	buttonColor.setHsv(
		buttonColor.hue(), buttonColor.saturation() * 0.75,
		buttonColor.value());
	return buttonColor;
}

QLinearGradient
FusionProxyStyle::qt_fusion_gradient(const QRect &rect, const QBrush &baseColor)
{
	int x = rect.center().x();
	QLinearGradient gradient = QLinearGradient(x, rect.top(), x, rect.bottom());
	if(baseColor.gradient())
		gradient.setStops(baseColor.gradient()->stops());
	else {
		QColor gradientStartColor = baseColor.color().lighter(124);
		QColor gradientStopColor = baseColor.color().lighter(102);
		gradient.setColorAt(0, gradientStartColor);
		gradient.setColorAt(1, gradientStopColor);
	}
	return gradient;
}

// SPDX-SnippetEnd

}
