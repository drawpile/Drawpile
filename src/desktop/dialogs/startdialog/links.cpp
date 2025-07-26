// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/links.h"
#include "desktop/main.h"
#include "desktop/settings.h"
#include "desktop/utils/widgetutils.h"
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QPushButton>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QVector>

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

	QVector<LinkDefinition> linkDefs = {
		{"love", QCoreApplication::translate("donations", "Donate"),
		 QCoreApplication::translate(
			 "donations", "Open Drawpile's donate page in your browser"),
		 QUrl(utils::getDonationLink())},
		{"help-contents", tr("Help"),
		 tr("Open Drawpile's help pages in your browser"),
		 QUrl{utils::getHelpLink()}},
		{"input-tablet", tr("Tablet Setup"),
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
		for(int i = 0, count = linkDefs.size(); i < count; ++i) {
			const LinkDefinition &ld = linkDefs[i];
			QPushButton *link = new QPushButton;
			link->setStyleSheet(pushButtonCss);
			link->setFlat(true);
			setUpLink(i, ld, link);
		}
	} else {
		QString toolButtonCss = QStringLiteral("QToolButton {"
											   "	text-decoration: underline;"
											   "}");
		for(int i = 0, count = linkDefs.size(); i < count; ++i) {
			const LinkDefinition &ld = linkDefs[i];
			QToolButton *link = new QToolButton;
			link->setStyleSheet(toolButtonCss);
			link->setAutoRaise(true);
			link->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
			setUpLink(i, ld, link);
		}
	}

	layout->addStretch();
}

void Links::setUpLink(
	int index, const LinkDefinition &ld, QAbstractButton *link)
{
	link->setIcon(QIcon::fromTheme(ld.icon));
	link->setIconSize(QSize{24, 24});
	link->setText(ld.title);
	link->setToolTip(ld.toolTip);
	link->setCursor(Qt::PointingHandCursor);
	connect(link, &QAbstractButton::clicked, this, [url = ld.url] {
		QDesktopServices::openUrl(url);
	});

	if(index == DONATION_LINK_INDEX) {
		dpApp().settings().bindDonationLinksEnabled(link, [link](bool enabled) {
			link->setEnabled(enabled);
			link->setVisible(enabled);
		});
	}

	layout()->addWidget(link);
}

}
}
