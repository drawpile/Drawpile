// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_STARTDIALOG_H
#define DESKTOP_DIALOGS_STARTDIALOG_H

#include "libclient/utils/news.h"
#include <QDialog>
#include <QVector>

class QAbstractButton;
class QAction;
class QFrame;
class QIcon;
class QShortcut;
class QStackedWidget;
class QTimer;
class QUrl;

namespace dialogs {

namespace startdialog {
class Links;
class Page;
class UpdateNotice;
}

class StartDialog final : public QDialog {
	Q_OBJECT
public:
	enum Entry : int {
		Welcome,
		Join,
		Browse,
		Host,
		Create,
		Recent,
		Open,
		Layouts,
		Preferences,
		Count,
		Guess = Count,
	};
	Q_ENUM(Entry)

	struct Actions {
		const QAction *entries[Entry::Count];
	};

	StartDialog(QWidget *parent = nullptr);

	void setActions(const Actions &actions);

	void showPage(Entry entry);

	void autoJoin(const QUrl &url);

public slots:
	void checkForUpdates();

signals:
	void openFile();
	void openUrl(const QUrl &url);
	void layouts();
	void preferences();
	void join(const QUrl &url, const QString recordingFilename);
	void host(
		const QString &title, const QString &password, const QString &alias,
		bool nsfm, const QString &announcementUrl,
		const QString &remoteAddress);
	void create(const QSize &size, const QColor &backgroundColor);
	void joinAddressSet(const QString &address);
	void hostSessionEnabled(bool enabled);

protected:
	void resizeEvent(QResizeEvent *event) override;

private slots:
	void addListServer();
	void addListServerUrl(const QUrl &url);
	void toggleRecording(bool checked);
	void updateCheckForUpdatesButton(bool inProgress);
	void hideLinks();
	void showWelcomeButtons();
	void showJoinButtons();
	void showBrowseButtons();
	void showHostButtons();
	void showCreateButtons();
	void okClicked();
	void followLink(const QString &fragment);
	void joinRequested(const QUrl &url);
	void hostRequested(
		const QString &title, const QString &password, const QString &alias,
		bool nsfm, const QString &announcementUrl,
		const QString &remoteAddress);
	void rememberLastPage(int i);
	void initialUpdateDelayFinished();
	void setUpdate(const utils::News::Update &update);

private:
	static constexpr char ENTRY_PROPERTY_KEY[] = "startdialogentry";
	static constexpr int MAX_LAST_PAGE_REMEMBER_SECS = 60 * 60 * 24;
	static constexpr int CHECK_FOR_UPDATES_DELAY_MSEC = 1000;

	void entryClicked(Entry entry);
	void entryToggled(startdialog::Page *page, bool checked);

	void guessPage();

	static void addRecentHost(const QUrl &url, bool join);

	startdialog::UpdateNotice *m_updateNotice;
	QStackedWidget *m_stack;
	QFrame *m_linksSeparator;
	startdialog::Links *m_links;
	QAbstractButton *m_addServerButton;
	QAbstractButton *m_recordButton;
	QAbstractButton *m_checkForUpdatesButton;
	QAbstractButton *m_okButton;
	QAbstractButton *m_cancelButton;
	QAbstractButton *m_closeButton;
	startdialog::Page *m_currentPage = nullptr;
	QAbstractButton *m_buttons[Entry::Count];
	QVector<QShortcut *> m_shortcuts;
	QString m_recordingFilename;
	QTimer *m_initialUpdateDelayTimer;
	utils::News m_news;
	utils::News::Update m_update = utils::News::Update::invalid();
};

}

#endif
