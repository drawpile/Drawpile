// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_STARTDIALOG_WELCOME_H
#define DESKTOP_DIALOGS_STARTDIALOG_WELCOME_H

#include "desktop/dialogs/startdialog/page.h"

class QTextBrowser;

namespace dialogs {
namespace startdialog {

class Welcome final : public Page {
	Q_OBJECT
public:
	Welcome(QWidget *parent = nullptr);
	void activate() override;

public slots:
	void setNews(const QString &content);

signals:
	void showButtons();

private:
	QTextBrowser *m_browser;
};

}
}

#endif
