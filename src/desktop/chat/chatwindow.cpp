/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#include "desktop/chat/chatwindow.h"

#include <QVBoxLayout>

namespace widgets {

ChatWindow::ChatWindow(QWidget *content)
	: QWidget()
{
	setWindowTitle(tr("Chat"));

	setAttribute(Qt::WA_DeleteOnClose);
	auto *layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);
	layout->addWidget(content);
}

void ChatWindow::closeEvent(QCloseEvent *event)
{
	emit closing();
	QWidget::closeEvent(event);
}

}
