/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

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
