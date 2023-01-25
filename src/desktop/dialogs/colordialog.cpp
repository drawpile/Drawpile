/*
 * Drawpile - a collaborative drawing program.
 *
 * Copyright (C) 2023 askmeaboutloom
 *
 * Drawpile is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Drawpile is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
 */

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
