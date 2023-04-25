// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ACCOUNTLISTPAGE_H
#define ACCOUNTLISTPAGE_H

#include "thinsrv/gui/pagefactory.h"

#include <QWidget>
#include <QApplication>

namespace server {

struct JsonApiResult;

namespace gui {

class AccountListPage final : public QWidget
{
	Q_OBJECT
public:
	explicit AccountListPage(Server *server, QWidget *parent=nullptr);
	~AccountListPage() override;

private slots:
	void handleResponse(const QString &requestId, const JsonApiResult &result);

	void addNewAccount();
	void editSelectedAccount();
	void removeSelectedAccount();

private:
    static constexpr char REQ_ID[] = "accountlist";
    static constexpr char ADD_REQ_ID[] = "accountlistAdd";
    static constexpr char UPDATE_REQ_ID[] = "accountlistUpdate";
    static constexpr char DEL_REQ_ID[] = "accountlistDel";

	void refreshPage();

	struct Private;
	Private *d;
};

class AccountListPageFactory final : public PageFactory
{
public:
	QString pageId() const override { return QStringLiteral("accountlist"); }
	QString title() const override { return QApplication::tr("Accounts"); }

	AccountListPage *makePage(Server *server) const override { return new AccountListPage(server); }
};


}
}

#endif
