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

#ifndef COLORDIALOG_H
#define COLORDIALOG_H

#include <QtColorWidgets/ColorDialog>

namespace dialogs {

// Applies the user's color wheel settings to the given color dialog.
void applyColorDialogSettings(color_widgets::ColorDialog *dlg);

// Makes a color dialog with the user's color wheel settings applied.
color_widgets::ColorDialog *
newColorDialog(QWidget *parent);

// Same thing, but the dialog will have the given starting
// color and the Qt::WA_DeleteOnClose attribute set on it.
color_widgets::ColorDialog *
newDeleteOnCloseColorDialog(const QColor &color, QWidget *parent);

}

#endif
