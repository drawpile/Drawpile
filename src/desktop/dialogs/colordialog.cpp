// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSettings>
#include <QtColorWidgets/ColorDialog>

namespace dialogs {

void applyColorDialogSettings(color_widgets::ColorDialog *dlg)
{
	QSettings settings;
	settings.beginGroup("settings/colorwheel");
	dlg->setWheelShape(static_cast<color_widgets::ColorWheel::ShapeEnum>(
		settings.value("shape").toInt()));
	dlg->setWheelRotating(static_cast<color_widgets::ColorWheel::AngleEnum>(
		settings.value("rotate").toInt()));
	dlg->setColorSpace(static_cast<color_widgets::ColorWheel::ColorSpaceEnum>(
		settings.value("space").toInt()));
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
