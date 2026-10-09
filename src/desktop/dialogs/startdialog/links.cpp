// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/links.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/config/config.h"
#include <QDesktopServices>
#include <QGridLayout>
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
	int row;
	int column;
};

Links::Links(bool vertical, QWidget *parent)
	: QWidget{parent}
{
	QVector<LinkDefinition> linkDefs = {
		{"love", QCoreApplication::translate("donations", "Donate"),
		 QCoreApplication::translate(
			 "donations", "Open Drawpile's donate page in your browser"),
		 QUrl(utils::getDonationLink()), 0, 0},
		{"help-contents", tr("Help"),
		 tr("Open Drawpile's help pages in your browser"),
		 QUrl{utils::getHelpLink()}, 0, 1},
		{"input-tablet", tr("Tablet Setup"),
		 tr("Open Drawpile's tablet setup and troubleshooting help page"),
		 QUrl{"https://docs.drawpile.net/help/tech/tablet"}, -1, -1},
		{"user-group-new", tr("Communities"),
		 tr("Open Drawpile's communities page in your browser"),
		 QUrl{"https://drawpile.net/communities/"}, 0, 2},
		{"fa_discord", tr("Discord"), tr("Join the Drawpile Discord server"),
		 QUrl{"https://drawpile.net/discord/"}, 1, 0},
		{"irc-operator", tr("libera.chat"),
		 tr("Join the #drawpile chatroom on libera.chat"),
		 QUrl{"https://drawpile.net/irc/"}, 1, 1},
		{"fa_github", tr("GitHub"),
		 tr("Open Drawpile's GitHub page in your browser"),
		 QUrl{"https://github.com/drawpile/Drawpile#readme"}, 1, 2},
	};

	if(vertical) {
		QVBoxLayout *linksLayout = new QVBoxLayout(this);
		QString pushButtonCss = QStringLiteral(
			"QPushButton {"
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
			linksLayout->addWidget(link);
		}
	} else {
		QGridLayout *linksLayout = new QGridLayout(this);
		QString toolButtonCss = QStringLiteral(
			"QToolButton {"
			"	text-decoration: underline;"
			"}");
		for(int i = 0, count = linkDefs.size(); i < count; ++i) {
			const LinkDefinition &ld = linkDefs[i];
			if(ld.row >= 0 && ld.column >= 0) {
				QToolButton *link = new QToolButton;
				link->setStyleSheet(toolButtonCss);
				link->setAutoRaise(true);
				link->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
				link->setSizePolicy(
					QSizePolicy::Expanding, QSizePolicy::Expanding);
				setUpLink(i, ld, link);
				linksLayout->addWidget(link, ld.row, ld.column);
			}
		}
	}
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
		CFG_BIND_SET_FN(
			dpAppConfig(), DonationLinksEnabled, link, [link](bool enabled) {
				link->setEnabled(enabled);
				link->setVisible(enabled);
			});
	}
}

}
}
