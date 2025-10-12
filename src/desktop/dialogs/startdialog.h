// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_STARTDIALOG_H
#define DESKTOP_DIALOGS_STARTDIALOG_H
#include "desktop/utils/hostparams.h"
#include <QDialog>
#include <QVector>
#ifndef __EMSCRIPTEN__
#	include "libclient/utils/news.h"
#endif

class QAbstractButton;
class QAction;
class QFrame;
class QIcon;
class QMenu;
class QShortcut;
class QStackedWidget;
class QTimer;
class QToolButton;
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
#ifndef __EMSCRIPTEN__
		Recent,
#endif
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

	StartDialog(bool smallScreenMode, QWidget *parent = nullptr);

	void setActions(const Actions &actions);

	void showPage(Entry entry);

	void autoJoin(
		const QUrl &url, const QString &autoRecordPath, int connectStrategy);

public slots:
#ifndef __EMSCRIPTEN__
	void checkForUpdates();
#endif
	void setSmallScreenMode(bool smallScreenMode);

signals:
	void openFile();
	void openRecent(const QString &path);
	void layouts();
	void preferences();
	void networkPreferences();
	void
	join(const QUrl &url, const QString recordingFilename, int connectStrategy);
	void host(const HostParams &params, int connectStrategy);
	void create(const QSize &size, const QColor &backgroundColor);
	void joinAddressSet(const QString &address);
	void hostSessionEnabled(bool enabled);
	void hostPageEnabled(bool enabled);

protected:
	void resizeEvent(QResizeEvent *event) override;

private slots:
	void addListServer();
	void addListServerUrl(const QUrl &url);
	void toggleRecording(bool checked);
	void triggerReset();
	void triggerLoad();
	void triggerSave();
	void triggerImport();
	void triggerExport();
#ifndef __EMSCRIPTEN__
	void updateCheckForUpdatesButton(bool inProgress);
#endif
	void hideLinks();
	void showWelcomeButtons();
	void showJoinButtons();
	void showBrowseButtons();
	void showHostButtons();
	void showCreateButtons();
	void triggerAccept(int connectStrategy);
	void setJoinEnabled(bool enabled);
	void setHostEnabled(bool ok, bool webSocket, bool tcp);
	void followLink(const QString &fragment);
	void joinRequested(const QUrl &url);
	void hostRequested(const HostParams &params);
	void rememberLastPage(int i);
#ifndef __EMSCRIPTEN__
	void initialUpdateDelayFinished();
	void setUpdate(const utils::News::Update &update);
#endif

private:
	static constexpr char ENTRY_PROPERTY_KEY[] = "startdialogentry";
	static constexpr int MAX_LAST_PAGE_REMEMBER_SECS = 60 * 60 * 24;
	static constexpr int CHECK_FOR_UPDATES_DELAY_MSEC = 1000;

	void entryClicked(Entry entry);
	void entryToggled(startdialog::Page *page, bool checked);

	void guessPage();

	static void addRecentHost(const QUrl &url, bool join);

#ifndef __EMSCRIPTEN__
	startdialog::UpdateNotice *m_updateNotice;
#endif
	QStackedWidget *m_stack;
	QFrame *m_linksSeparator;
	startdialog::Links *m_links;
	QPushButton *m_addServerButton;
	QPushButton *m_recordButton;
	QPushButton *m_saveLoadButton;
#ifndef __EMSCRIPTEN__
	QAbstractButton *m_checkForUpdatesButton;
#endif
	QAbstractButton *m_okButton;
	QAbstractButton *m_cancelButton;
	QAbstractButton *m_closeButton;
	QToolButton *m_advancedButton;
	QMenu *m_advancedMenu;
	QAction *m_connectTcpAction;
	QAction *m_connectWebSocketAction;
	startdialog::Page *m_currentPage = nullptr;
	QAbstractButton *m_buttons[Entry::Count];
	QVector<QShortcut *> m_shortcuts;
	QString m_recordingFilename;
#ifndef __EMSCRIPTEN__
	QTimer *m_initialUpdateDelayTimer;
	utils::News m_news;
	utils::News::Update m_update = utils::News::Update::invalid();
#endif
	int m_connectStrategy;
};

}

#endif
