// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/main.h"
#include "desktop/settings.h"
#include <QtColorWidgets/ColorDialog>
#include <QtColorWidgets/ColorPreview>

namespace dialogs {

void applyColorDialogSettings(color_widgets::ColorDialog *dlg)
{
	auto &settings = dpApp().settings();
	settings.bindColorWheelShape(dlg, &color_widgets::ColorDialog::setWheelShape);
	settings.bindColorWheelAngle(dlg, &color_widgets::ColorDialog::setWheelRotating);
	settings.bindColorWheelMirror(dlg, &color_widgets::ColorDialog::setWheelMirrored);
}

void setColorDialogResetColor(color_widgets::ColorDialog *dlg, const QColor &color)
{
	color_widgets::ColorPreview *preview = dlg->findChild<color_widgets::ColorPreview *>(
		QStringLiteral("preview"), Qt::FindDirectChildrenOnly);
	if(preview) {
		preview->setComparisonColor(color);
		preview->setDisplayMode(color_widgets::ColorPreview::SplitColor);
	} else {
		qWarning("Preview not found in color dialog, reset color not set");
	}
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
