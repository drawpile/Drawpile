// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/startdialog/welcome.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/utils/news.h"
#include <QDesktopServices>
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

	m_browser = new QTextBrowser;
	m_browser->setOpenLinks(false);
	utils::bindKineticScrolling(m_browser);
	layout->addWidget(m_browser);

	connect(
		m_browser, &QTextBrowser::anchorClicked, this, &Welcome::linkClicked);
}

void Welcome::activate()
{
	dpApp().settings().setWelcomePageShown(true);
	emit showButtons();
}

void Welcome::showFirstStartText()
{
	m_browser->setText(
		QStringLiteral("<h1>%1</h1>"
					   "<p style=\"font-size:large;\">%2</p>"
					   "<p style=\"font-size:large;\">%3</p>"
					   "<p style=\"font-size:large;\">%4</p>"
					   "<p style=\"font-size:large;\">%5</p>")
			.arg(
				tr("Welcome to Drawpile!"),
				tr("Take a look over the "
				   "<a href=\"#preferences\">Preferences</a> to configure "
				   "things to your liking. You might also want to pick a "
				   "different <a href=\"#layouts\">Layout</a> for the "
				   "application."),
				tr("If you have an invite link to a drawing session, you can "
				   "<a href=\"#join\">Join</a> it directly. Alternatively, "
				   "you can <a href=\"#browse\">Browse</a> public sessions or "
				   "just create a <a href=\"#create\">New Canvas</a> to draw "
				   "on your own."),
				tr("If you have trouble, take a look at the help pages linked "
				   "on the right. If they don't answer your question, you can "
				   "join the Discord server and ask, there's usually folks "
				   "around to help!"),
				tr("Next time, you'll see news and updates on this page.")));
}

void Welcome::setNews(const QString &content)
{
	m_browser->setText(
		QStringLiteral(
			"<table><tr><td style=\"padding:4px;\">%1</td></tr></table>")
			.arg(content));
}

void Welcome::linkClicked(const QUrl &url)
{
	if(url.scheme().isEmpty() && url.path().isEmpty()) {
		emit linkActivated(url.fragment());
	} else {
		QDesktopServices::openUrl(url);
	}
}

}
}
