// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DOCKS_COLORSPINNER_H
#define DESKTOP_DOCKS_COLORSPINNER_H
#include "desktop/docks/dockbase.h"
#include <QtColorWidgets/color_wheel.hpp>

namespace color_widgets {
class ColorPalette;
}

namespace docks {

class ColorSpinnerDock final : public DockBase {
	Q_OBJECT
public:
	ColorSpinnerDock(const QString &title, QWidget *parent);
	~ColorSpinnerDock() override;

	void showPreviewPopup();
	void hidePreviewPopup();

public slots:
	void setColor(const QColor &color);
	void setLastUsedColors(const color_widgets::ColorPalette &pal);
	void setShape(color_widgets::ColorWheel::ShapeEnum shape);
	void setAngle(color_widgets::ColorWheel::AngleEnum angle);
	void setColorSpace(color_widgets::ColorWheel::ColorSpaceEnum colorSpace);
	void setMirror(bool mirror);
	void setAlign(int align);
	void setPreview(int preview);

signals:
	void colorSelected(const QColor &color);

private:
	void updateShapeAction();

	struct Private;
	Private *d;
};

}

#endif
