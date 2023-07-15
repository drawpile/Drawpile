// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/startdialog/welcome.h"

#include <QTextBrowser>
#include <QVBoxLayout>

namespace dialogs {
namespace startdialog {

Welcome::Welcome(QWidget *parent)
	: Page{parent}
{
	QVBoxLayout *layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);

	QTextBrowser *browser = new QTextBrowser;
	browser->setOpenExternalLinks(true);
	layout->addWidget(browser);

	browser->setText(QStringLiteral(
		"<h1>New Start Dialog</h1><p>There'll be news and update notifications "
		"here later, but for now enjoy this placeholder text.</p><p>This "
		"dialog will now show up on startup to give you quick access to the "
		"various ways of starting a drawing session through the buttons on the "
		"left, rather than just dumping you into a blank document and making "
		"you pick through the menus.</p>"));
}

void Welcome::activate()
{
	emit showButtons();
}

}
}
