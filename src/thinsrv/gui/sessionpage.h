// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SESSIONPAGE_H
#define SESSIONPAGE_H

#include "thinsrv/gui/pagefactory.h"

#include <QWidget>

namespace server {

struct JsonApiResult;

namespace gui {

class SessionPage final : public QWidget
{
	Q_OBJECT
public:
	struct Private;

	explicit SessionPage(Server *server, const QString &id, QWidget *parent=nullptr);
	~SessionPage() override;

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

class SessionPageFactory final : public PageFactory
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
