// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_STARTDIALOG_WELCOME_H
#define DESKTOP_DIALOGS_STARTDIALOG_WELCOME_H

#include "desktop/dialogs/startdialog/page.h"

class QTextBrowser;
class QUrl;

namespace dialogs {
namespace startdialog {

class Welcome final : public Page {
	Q_OBJECT
public:
	Welcome(QWidget *parent = nullptr);
	void activate() override;

#ifdef __EMSCRIPTEN__
	void showStandaloneText();
#else
	void showFirstStartText();
#endif

public slots:
#ifndef __EMSCRIPTEN__
	void setNews(const QString &content);
#endif

signals:
	void showButtons();
	void linkActivated(const QString &fragment);

private slots:
	void linkClicked(const QUrl &url);

private:
	QTextBrowser *m_browser;
};

}
}

#endif
