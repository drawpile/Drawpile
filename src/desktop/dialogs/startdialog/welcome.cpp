// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/startdialog/welcome.h"
#include "desktop/main.h"
#include "libclient/utils/news.h"

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
	m_browser->setOpenExternalLinks(true);
	layout->addWidget(m_browser);
}

void Welcome::activate()
{
	emit showButtons();
}

void Welcome::setNews(const QString &content)
{
	m_browser->setText(content);
}


}
}
