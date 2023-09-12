// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COLORDIALOG_H
#define COLORDIALOG_H

#include <QtColorWidgets/ColorDialog>

namespace dialogs {

// Applies the user's color wheel settings to the given color dialog.
void applyColorDialogSettings(color_widgets::ColorDialog *dlg);

// Sets the color that the Reset button inside the dialog will reset to.
// Also changes the preview to split mode, to make it clear what reset will do.
void setColorDialogResetColor(
	color_widgets::ColorDialog *dlg, const QColor &color);

// Makes a color dialog with the user's color wheel settings applied.
color_widgets::ColorDialog *
newColorDialog(QWidget *parent);

// Same thing, but the dialog will have the given starting
// color and the Qt::WA_DeleteOnClose attribute set on it.
color_widgets::ColorDialog *
newDeleteOnCloseColorDialog(const QColor &color, QWidget *parent);

}

#endif
