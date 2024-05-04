// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/links.h"
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QPushButton>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace dialogs {
namespace startdialog {

struct Links::LinkDefinition {
	QString icon;
	QString title;
	QString toolTip;
	QUrl url;
};

Links::Links(bool vertical, QWidget *parent)
	: QWidget{parent}
{
	QBoxLayout *layout = new QBoxLayout(
		vertical ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
	setLayout(layout);

	LinkDefinition linkDefs[] = {
		{"help-contents", tr("Help"),
		 tr("Open Drawpile's help pages in your browser"),
		 QUrl{"https://drawpile.net/help/"}},
		{"dialog-input-devices", tr("Tablet Setup"),
		 tr("Open Drawpile's tablet setup and troubleshooting help page"),
		 QUrl{"https://docs.drawpile.net/help/tech/tablet"}},
		{"user-group-new", tr("Communities"),
		 tr("Open Drawpile's communities page in your browser"),
		 QUrl{"https://drawpile.net/communities/"}},
		{"fa_discord", tr("Discord"), tr("Join the Drawpile Discord server"),
		 QUrl{"https://drawpile.net/discord/"}},
		{"irc-operator", tr("libera.chat"),
		 tr("Join the #drawpile chatroom on libera.chat"),
		 QUrl{"https://drawpile.net/irc/"}},
		{"fa_github", tr("GitHub"),
		 tr("Open Drawpile's GitHub page in your browser"),
		 QUrl{"https://github.com/drawpile/Drawpile#readme"}},
	};

	if(vertical) {
		QString pushButtonCss = QStringLiteral("QPushButton {"
											   "	font-size: 20px;"
											   "	text-decoration: underline;"
											   "	text-align: left;"
											   "}");
		for(const LinkDefinition &ld : linkDefs) {
			QPushButton *link = new QPushButton;
			link->setStyleSheet(pushButtonCss);
			link->setFlat(true);
			setUpLink(ld, link);
		}
	} else {
		QString toolButtonCss = QStringLiteral("QToolButton {"
											   "	text-decoration: underline;"
											   "}");
		for(const LinkDefinition &ld : linkDefs) {
			QToolButton *link = new QToolButton;
			link->setStyleSheet(toolButtonCss);
			link->setAutoRaise(true);
			link->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
			setUpLink(ld, link);
		}
	}

	layout->addStretch();
}

void Links::setUpLink(const LinkDefinition &ld, QAbstractButton *link)
{
	link->setIcon(QIcon::fromTheme(ld.icon));
	link->setIconSize(QSize{24, 24});
	link->setText(ld.title);
	link->setToolTip(ld.toolTip);
	link->setCursor(Qt::PointingHandCursor);
	connect(link, &QAbstractButton::clicked, this, [url = ld.url] {
		QDesktopServices::openUrl(url);
	});
	layout()->addWidget(link);
}

}
}
