// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/startdialog/page.h"

namespace dialogs {
namespace startdialog {

Page::Page(QWidget *parent)
	: QWidget{parent}
{
}

void Page::activate()
{
	// Override me.
}

void Page::accept()
{
	// Override me.
}

}
}
