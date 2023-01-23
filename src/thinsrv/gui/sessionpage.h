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

#ifndef SESSIONPAGE_H
#define SESSIONPAGE_H

#include "thinsrv/gui/pagefactory.h"

#include <QWidget>

namespace  server {

struct JsonApiResult;

namespace gui {

class SessionPage : public QWidget
{
	Q_OBJECT
public:
	struct Private;

	explicit SessionPage(Server *server, const QString &id, QWidget *parent=nullptr);
	~SessionPage();

private slots:
	void saveSettings();
	void terminateSession();
	void changePassword();
	void changeTitle();
	void sendMessage();
	void kickUser();
	void removeAnnouncement();
	void sendUserMessage();
	void handleResponse(const QString &requestId, const JsonApiResult &result);

private:
	void refreshPage();
	int selectedUser() const;
	int selectedAnnouncement() const;

	Private *d;
};

class SessionPageFactory : public PageFactory
{
public:
	SessionPageFactory(const QString &id) : m_id(id) { }
	QString pageId() const override { return QStringLiteral("session:") + m_id; }
	QString title() const override { return m_id; }

	SessionPage *makePage(Server *server) const override { return new SessionPage(server, m_id); }

private:
	QString m_id;
};


}
}

#endif
