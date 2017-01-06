/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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
#ifndef SERVERSUMMARYPAGE_H
#define SERVERSUMMARYPAGE_H

#include "pagefactory.h"

#include <QWidget>
#include <QApplication>

namespace server {
namespace gui {

class ServerSummaryPage : public QWidget
{
	Q_OBJECT
public:
	explicit ServerSummaryPage(Server *server, QWidget *parent=nullptr);
	~ServerSummaryPage();

private slots:
	void startOrStopServer();

private:
	void refreshPage();

	struct Private;
	Private *d;
};

class ServersummaryPageFactory : public PageFactory
{
public:
	QString pageId() const override { return QStringLiteral("summary:server"); }
	QString title() const override { return QApplication::tr("Server"); }
	ServerSummaryPage *makePage(Server *server) const { return new ServerSummaryPage(server); }
};

}
}

#endif
