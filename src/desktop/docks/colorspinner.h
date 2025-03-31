// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DOCKS_COLORSPINNER_H
#define DESKTOP_DOCKS_COLORSPINNER_H
#include "desktop/docks/dockbase.h"
#include <QtColorWidgets/color_wheel.hpp>

// TODO: On Android, the color popup ends up blocking inputs to the main window.
// We're disabling it for now, but presumably this can be fixed, since a similar
// thing is working fine on Krita for Android. It also works if the Qt::Window
// window flag is used instead of Qt::Popup, but then it covers the screen.
// TODO: Dito on macOS.
// TODO: On Emscripten, the window just doesn't show up for some reason.
#if defined(Q_OS_ANDROID) || defined(Q_OS_MACOS) || defined(__EMSCRIPTEN__)
#	undef DP_COLOR_SPINNER_ENABLE_PREVIEW
#else
#	define DP_COLOR_SPINNER_ENABLE_PREVIEW
#endif

namespace color_widgets {
class ColorPalette;
}

namespace docks {

class ColorSpinnerDock final : public DockBase {
	Q_OBJECT
public:
	explicit ColorSpinnerDock(QWidget *parent);
	~ColorSpinnerDock() override;

#ifdef DP_COLOR_SPINNER_ENABLE_PREVIEW
	void showPreviewPopup();
	void hidePreviewPopup();
#endif

public slots:
	void setColor(const QColor &color);
	void setLastUsedColors(const color_widgets::ColorPalette &pal);
	void setShape(color_widgets::ColorWheel::ShapeEnum shape);
	void setAngle(color_widgets::ColorWheel::AngleEnum angle);
	void setColorSpace(color_widgets::ColorWheel::ColorSpaceEnum colorSpace);
	void setMirror(bool mirror);
	void setAlign(int align);
#ifdef DP_COLOR_SPINNER_ENABLE_PREVIEW
	void setPreview(int preview);
#endif

signals:
	void colorSelected(const QColor &color);

private:
	void updateShapeAction();
	void setSwatchFlags(int flags);

	struct Private;
	Private *d;
};

}

#endif
