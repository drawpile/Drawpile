// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SESSIONLISTPAGE_H
#define SESSIONLISTPAGE_H

#include "thinsrv/gui/pagefactory.h"

#include <QWidget>
#include <QApplication>

namespace server {

struct JsonApiResult;

namespace gui {

class SessionListModel;

class SessionListPage final : public QWidget
{
	Q_OBJECT
public:
	explicit SessionListPage(Server *server, QWidget *parent=nullptr);
	~SessionListPage() override;

private slots:
	void sendMessageToAll();

private:
	struct Private;
	Private *d;
};

class SessionListPageFactory final : public PageFactory
{
public:
	QString pageId() const override { return QStringLiteral("sessionlist"); }
	QString title() const override { return QApplication::tr("Sessions"); }

	SessionListPage *makePage(Server *server) const override { return new SessionListPage(server); }
};


}
}

#endif
