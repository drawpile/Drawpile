// SPDX-License-Identifier: GPL-3.0-or-later

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
