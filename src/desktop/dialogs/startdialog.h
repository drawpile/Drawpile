// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_STARTDIALOG_H
#define DESKTOP_DIALOGS_STARTDIALOG_H

#include <QDialog>

class QAbstractButton;
class QFrame;
class QIcon;
class QStackedWidget;
class QUrl;

namespace dialogs {

namespace startdialog {
class Links;
class Page;
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
		Open,
		Layouts,
		Preferences,
		Count,
	};
	Q_ENUM(Entry)

	StartDialog(QWidget *parent = nullptr);

	void showPage(Entry entry);

	void autoJoin(const QUrl &url);

signals:
	void openFile();
	void layouts();
	void preferences();
	void join(const QUrl &url, const QString recordingFilename);
	void host(
		const QString &title, const QString &password, const QString &alias,
		bool nsfm, const QString &announcementUrl,
		const QString &remoteAddress);
	void create(const QSize &size, const QColor &backgroundColor);
	void joinAddressSet(const QString &address);

protected:
	void resizeEvent(QResizeEvent *event) override;

private slots:
	void addListServer();
	void addListServerUrl(const QUrl &url);
	void toggleRecording(bool checked);
	void hideLinks();
	void showWelcomeButtons();
	void showJoinButtons();
	void showBrowseButtons();
	void showHostButtons();
	void showCreateButtons();
	void okClicked();
	void joinRequested(const QUrl &url);
	void hostRequested(
		const QString &title, const QString &password, const QString &alias,
		bool nsfm, const QString &announcementUrl,
		const QString &remoteAddress);

private:
	void entryClicked(Entry entry);
	void entryToggled(startdialog::Page *page, bool checked);

	static void addRecentHost(const QUrl &url, bool join);

	QStackedWidget *m_stack;
	QFrame *m_linksSeparator;
	startdialog::Links *m_links;
	QAbstractButton *m_addServerButton;
	QAbstractButton *m_recordButton;
	QAbstractButton *m_okButton;
	QAbstractButton *m_cancelButton;
	QAbstractButton *m_closeButton;
	startdialog::Page *m_currentPage = nullptr;
	QAbstractButton *m_buttons[Entry::Count];
	QString m_recordingFilename;
};

}

#endif
