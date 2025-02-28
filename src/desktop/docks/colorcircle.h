// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DOCKS_COLORCIRCLE_H
#define DESKTOP_DOCKS_COLORCIRCLE_H
#include "desktop/docks/dockbase.h"
#include <QColor>
#include <QtColorWidgets/color_wheel.hpp>

// TODO: On Android, the color popup ends up blocking inputs to the main window.
// We're disabling it for now, but presumably this can be fixed, since a similar
// thing is working fine on Krita for Android. It also works if the Qt::Window
// window flag is used instead of Qt::Popup, but then it covers the screen.
// TODO: On Emscripten, the window just doesn't show up for some reason.
#if defined(Q_OS_ANDROID) || defined(__EMSCRIPTEN__)
#	undef DP_COLOR_CIRCLE_ENABLE_PREVIEW
#else
#	define DP_COLOR_CIRCLE_ENABLE_PREVIEW
#endif

class QAction;

namespace color_widgets {
class ColorPalette;
class Swatch;
}

namespace widgets {
class ArtisticColorWheel;
class ColorPopup;
}

namespace docks {

class ColorCircleDock final : public DockBase {
	Q_OBJECT
public:
	explicit ColorCircleDock(QWidget *parent);

	void setColor(const QColor &color);
	void setLastUsedColors(const color_widgets::ColorPalette &pal);
	void setColorSpace(color_widgets::ColorWheel::ColorSpaceEnum colorSpace);

#ifdef DP_COLOR_CIRCLE_ENABLE_PREVIEW
	void setPreview(int preview);
	void showPreviewPopup();
	void hidePreviewPopup();
#endif

signals:
	void colorSelected(const QColor &color);

private:
	void showSettingsDialog();
	void setSwatchFlags(int flags);

	QAction *m_colorSpaceHsvAction = nullptr;
	QAction *m_colorSpaceHslAction = nullptr;
	QAction *m_colorSpaceHclAction = nullptr;
#ifdef DP_COLOR_CIRCLE_ENABLE_PREVIEW
	QAction *m_previewAction = nullptr;
	widgets::ColorPopup *m_popup = nullptr;
	bool m_popupEnabled = false;
#endif
	color_widgets::Swatch *m_lastUsedSwatch;
	QColor m_lastUsedColor;
	widgets::ArtisticColorWheel *m_wheel;
	bool m_updating = false;
};

}

#endif
