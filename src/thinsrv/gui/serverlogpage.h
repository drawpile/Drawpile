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

#ifndef SERVERLOGPAGE_H
#define SERVERLOGPAGE_H

#include "thinsrv/gui/pagefactory.h"

#include <QWidget>
#include <QApplication>

namespace server {

struct JsonApiResult;

namespace gui {

class ServerLogPage final : public QWidget
{
	Q_OBJECT
public:
	explicit ServerLogPage(Server *server, QWidget *parent=nullptr);
	~ServerLogPage() override;

private slots:
	void handleResponse(const QString &requestId, const JsonApiResult &result);

private:
	void refreshPage();

	struct Private;
	Private *d;
};

class ServerLogPageFactory final : public PageFactory
{
public:
	QString pageId() const override { return QStringLiteral("serverlog"); }
	QString title() const override { return QApplication::tr("Server log"); }

	ServerLogPage *makePage(Server *server) const override { return new ServerLogPage(server); }
};


}
}

#endif
