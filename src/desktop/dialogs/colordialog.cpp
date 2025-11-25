// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/config/config.h"
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QtColorWidgets/ColorDialog>
#include <QtColorWidgets/ColorPreview>
#include <QtColorWidgets/ColorWheel>
#include <functional>

namespace dialogs {

#ifdef Q_OS_LINUX
void showColorPickUnsupportedMessage(QWidget *parent)
{
	utils::showWarning(
		parent,
		QCoreApplication::translate(
			"tools::ColorPickerSettings", "Screen Color Picking Unsupported"),
		//: Wayland and Xorg are the names so-called display servers on
		//: Linux. You don't need to worry about what that is, just don't
		//: change the names.
		QCoreApplication::translate(
			"tools::ColorPickerSettings",
			"Picking colors from the screen is not supported under Wayland."),
		//: Wayland and Xorg are the names so-called display servers on
		//: Linux. You don't need to worry about what that is, just don't
		//: change the names. Note that "Linux user account" is an account
		//: on the computer, NOT a Drawpile account.
		QCoreApplication::translate(
			"tools::ColorPickerSettings",
			"You may be able to switch to Xorg instead when logging into your "
			"Linux user account."));
}
#endif

void applyColorDialogSettings(color_widgets::ColorDialog *dlg)
{
	config::Config *cfg = dpAppConfig();
	CFG_BIND_SET_FN(cfg, ColorWheelShape, dlg, [dlg](int shape) {
		dlg->setWheelShape(color_widgets::ColorWheel::ShapeEnum(shape));
	});
	CFG_BIND_SET_FN(cfg, ColorWheelAngle, dlg, [dlg](int angle) {
		dlg->setWheelRotating(color_widgets::ColorWheel::AngleEnum(angle));
	});
	CFG_BIND_SET(
		cfg, ColorWheelMirror, dlg,
		color_widgets::ColorDialog::setWheelMirrored);

	color_widgets::ColorWheel *wheel =
		dlg->findChild<color_widgets::ColorWheel *>(
			QStringLiteral("wheel"), Qt::FindDirectChildrenOnly);
	if(wheel) {
		utils::setWidgetLongPressEnabled(wheel, false);
	} else {
		qWarning("Wheel not found in color dialog, long presses not disabled");
	}

#if defined(Q_OS_ANDROID) || defined(__EMSCRIPTEN__)
	bool shouldRemovePickButton = true;
#elif defined(Q_OS_LINUX)
	bool shouldRemovePickButton =
		QGuiApplication::platformName() == QStringLiteral("wayland");
#else
	bool shouldRemovePickButton = false;
#endif
	if(shouldRemovePickButton) {
		QDialogButtonBox *buttonBox = dlg->findChild<QDialogButtonBox *>(
			QStringLiteral("buttonBox"), Qt::FindDirectChildrenOnly);
		if(buttonBox) {
#ifdef Q_OS_LINUX
			bool havePickButton = false;
			QString pickText;
			QIcon pickIcon;
#endif
			for(QAbstractButton *button : buttonBox->buttons()) {
				if(buttonBox->buttonRole(button) ==
				   QDialogButtonBox::ActionRole) {
#ifdef Q_OS_LINUX
					havePickButton = true;
					pickText = button->text();
					pickIcon = button->icon();
#endif
					buttonBox->removeButton(button);
					button->deleteLater();
				}
			}
#ifdef Q_OS_LINUX
			if(havePickButton) {
				QPushButton *pickButton = buttonBox->addButton(
					pickText, QDialogButtonBox::DestructiveRole);
				pickButton->setIcon(pickIcon);
				QObject::connect(
					pickButton, &QPushButton::clicked, dlg,
					std::bind(&showColorPickUnsupportedMessage, dlg));
			}
#endif
		} else {
			qWarning(
				"Button box not found in color dialog, not removing pick "
				"button");
		}
	}
}

void setColorDialogResetColor(
	color_widgets::ColorDialog *dlg, const QColor &color)
{
	color_widgets::ColorPreview *preview =
		dlg->findChild<color_widgets::ColorPreview *>(
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
