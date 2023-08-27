// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/main.h"
#include <QtColorWidgets/ColorDialog>

namespace dialogs {

void applyColorDialogSettings(color_widgets::ColorDialog *dlg)
{
	auto &settings = dpApp().settings();
	settings.bindColorWheelShape(dlg, &color_widgets::ColorDialog::setWheelShape);
	settings.bindColorWheelAngle(dlg, &color_widgets::ColorDialog::setWheelRotating);
	settings.bindColorWheelSpace(dlg, &color_widgets::ColorDialog::setColorSpace);
	settings.bindColorWheelMirror(dlg, &color_widgets::ColorDialog::setWheelMirrored);
}

color_widgets::ColorDialog *newColorDialog(QWidget *parent)
{
	color_widgets::ColorDialog *dlg = new color_widgets::ColorDialog{parent};
	applyColorDialogSettings(dlg);
	return dlg;
}

color_widgets::ColorDialog *
newDeleteOnCloseColorDialog(const QColor &color, QWidget *parent)
{
	color_widgets::ColorDialog *dlg = newColorDialog(parent);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setColor(color);
	return dlg;
}

}
