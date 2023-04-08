// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/chat/chatwidgetpinnedarea.h"
#include "libclient/utils/html.h"

namespace widgets {

ChatWidgetPinnedArea::ChatWidgetPinnedArea(QWidget *parent) :
	QLabel(parent)
{
	setVisible(false);
	setOpenExternalLinks(true);
	setWordWrap(true);
	setStyleSheet(QStringLiteral(
		"background: #232629;"
		"border-bottom: 1px solid #2980b9;"
		"color: #eff0f1;"
		"padding: 3px;"
					  ));
}

void ChatWidgetPinnedArea::setPinText(const QString &safetext)
{
	if(safetext.isEmpty()) {
		setVisible(false);
		setText(QString());
	} else {
		const QString htmltext = htmlutils::linkify(safetext, QStringLiteral("style=\"color:#3daae9\""));
		if (QString::compare(text(), htmltext)) {
			setText(htmltext);
			setVisible(true);
		}
	}
}

void ChatWidgetPinnedArea::mouseDoubleClickEvent(QMouseEvent *)
{
	setVisible(false);
}

}
