// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef USERLISTPAGE_H
#define USERLISTPAGE_H

#include "thinsrv/gui/pagefactory.h"

#include <QWidget>
#include <QApplication>

namespace server {

struct JsonApiResult;

namespace gui {

class UserListPage final : public QWidget
{
	Q_OBJECT
public:
	explicit UserListPage(Server *server, QWidget *parent=nullptr);
	~UserListPage() override;

private slots:
	void handleResponse(const QString &requestId, const JsonApiResult &result);

private:
	void refreshPage();

	struct Private;
	Private *d;
};

class UserListPageFactory final : public PageFactory
{
public:
	QString pageId() const override { return QStringLiteral("summary:users"); }
	QString title() const override { return QApplication::tr("Users"); }
	UserListPage *makePage(Server *server) const override { return new UserListPage(server); }
};

}
}

#endif
