// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/startdialog/recent.h"
#include "desktop/main.h"
#include "desktop/widgets/recentscroll.h"
#include <QVBoxLayout>

namespace dialogs {
namespace startdialog {

Recent::Recent(QWidget *parent)
	: Page{parent}
{
	QVBoxLayout *layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	setLayout(layout);

	m_recentScroll =
		new widgets::RecentScroll{widgets::RecentScroll::Mode::Files};
	connect(
		m_recentScroll, &widgets::RecentScroll::clicked, this,
		&Recent::recentFileSelected);
	layout->addWidget(m_recentScroll);
}

void Recent::recentFileSelected(const QString &path)
{
	emit openUrl(QUrl::fromLocalFile(path));
}

}
}
