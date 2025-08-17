// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/macmenu.h"
#include "desktop/dialogs/startdialog.h"
#include "desktop/main.h"
#include "desktop/mainwindow.h"
#include "desktop/utils/recents.h"
#include "desktop/utils/widgetutils.h"
#include <QMessageBox>
#include <QUrl>

MacMenu *MacMenu::instance()
{
	static MacMenu *menu;
	if(!menu) {
		menu = new MacMenu;
	}
	return menu;
}

MacMenu::MacMenu()
	: QMenuBar(nullptr)
{
	QMenu *filemenu = addMenu(MainWindow::tr("&File"));

	QAction *newdocument = makeAction(
		filemenu, "newdocument", MainWindow::tr("&New"), QKeySequence::New);
	QAction *open = makeAction(
		filemenu, "opendocument", MainWindow::tr("&Open…"), QKeySequence::Open);
	m_recent = filemenu->addMenu(MainWindow::tr("Open &Recent"));
	filemenu->addSeparator();
	QAction *start = makeAction(filemenu, "start", MainWindow::tr("&Start…"));

	connect(newdocument, &QAction::triggered, this, &MacMenu::newDocument);
	connect(open, &QAction::triggered, this, &MacMenu::openDocument);
	connect(m_recent, &QMenu::triggered, this, &MacMenu::openRecent);
	connect(start, &QAction::triggered, this, &MacMenu::startDocument);

	// Relocated menu items
	QAction *quit =
		makeAction(filemenu, "exitprogram", MainWindow::tr("&Quit"));
	QAction *macquit = makeAction(
		filemenu, "macexitprogram", MainWindow::tr("&Quit"),
		QKeySequence("Ctrl+Q"), QAction::QuitRole);
	connect(quit, &QAction::triggered, this, &MacMenu::quitAll);
	connect(macquit, &QAction::triggered, this, &MacMenu::quitAll);

	//
	// Edit menu
	//

	QMenu *editmenu = addMenu(MainWindow::tr("&Edit"));
	QAction *preferences =
		makeAction(editmenu, nullptr, MainWindow::tr("Prefere&nces"));
	QAction *macpreferences = makeAction(
		editmenu, nullptr, MainWindow::tr("Prefere&nces"),
		QAction::PreferencesRole);
	connect(preferences, &QAction::triggered, this, &MacMenu::showSettings);
	connect(macpreferences, &QAction::triggered, this, &MacMenu::showSettings);

	//
	// Session menu
	//

	QMenu *sessionmenu = addMenu(MainWindow::tr("&Session"));
	QAction *host =
		makeAction(sessionmenu, "hostsession", MainWindow::tr("&Host…"));
	QAction *join =
		makeAction(sessionmenu, "joinsession", MainWindow::tr("&Join…"));
	QAction *browse =
		makeAction(sessionmenu, "browsesession", MainWindow::tr("&Browse…"));

	connect(host, &QAction::triggered, this, &MacMenu::hostSession);
	connect(join, &QAction::triggered, this, &MacMenu::joinSession);
	connect(browse, &QAction::triggered, this, &MacMenu::browseSessions);

	//
	// Window menu (Mac specific)
	//
	m_windows = addMenu(MainWindow::tr("Window"));
	connect(m_windows, &QMenu::triggered, this, &MacMenu::winSelect);
	connect(m_windows, &QMenu::aboutToShow, this, &MacMenu::updateWinMenu);

	QAction *minimize =
		makeAction(m_windows, nullptr, tr("Minimize"), QKeySequence("Ctrl+M"));

	m_windows->addSeparator();

	connect(minimize, &QAction::triggered, this, &MacMenu::winMinimize);

	//
	// Help menu
	//
	QMenu *helpmenu = addMenu(MainWindow::tr("&Help"));

	QAction *homepage =
		makeAction(helpmenu, "dphomepage", MainWindow::tr("&Homepage"));
	QAction *donate = makeAction(
		helpmenu, "dpdonate",
		QCoreApplication::translate("donations", "Donate"));
	helpmenu->addSeparator();
	QAction *about =
		makeAction(helpmenu, "dpabout", MainWindow::tr("&About Drawpile"));
	QAction *macabout = makeAction(
		helpmenu, "macdpabout", MainWindow::tr("&About Drawpile"),
		QAction::AboutRole);
	QAction *aboutqt =
		makeAction(helpmenu, "aboutqt", MainWindow::tr("About &Qt"));
	QAction *macaboutqt = makeAction(
		helpmenu, "macaboutqt", MainWindow::tr("About &Qt"),
		QAction::AboutQtRole);
	helpmenu->addSeparator();
	QAction *versioncheck = makeAction(
		helpmenu, "versioncheck", MainWindow::tr("Check For Updates"));

	connect(homepage, &QAction::triggered, &MainWindow::homepage);
	connect(donate, &QAction::triggered, &MainWindow::donate);
	connect(about, &QAction::triggered, &MainWindow::about);
	connect(macabout, &QAction::triggered, &MainWindow::about);
	connect(aboutqt, &QAction::triggered, &QApplication::aboutQt);
	connect(macaboutqt, &QAction::triggered, &QApplication::aboutQt);
	connect(versioncheck, &QAction::triggered, this, &MacMenu::checkForUpdates);

	dpApp().recents().bindFileMenu(m_recent);
}

QAction *MacMenu::makeAction(
	QMenu *menu, const char *name, const QString &text,
	const QKeySequence &shortcut, QAction::MenuRole menuRole)
{
	QAction *act;
	act = new QAction(text, this);
	act->setMenuRole(menuRole);

	if(name) {
		act->setObjectName(name);
	}

	if(!shortcut.isEmpty()) {
		act->setShortcut(shortcut);
	}

	menu->addAction(act);
	return act;
}

QAction *MacMenu::makeAction(
	QMenu *menu, const char *name, const QString &text,
	QAction::MenuRole menuRole)
{
	return makeAction(menu, name, text, QKeySequence(), menuRole);
}

void MacMenu::newDocument()
{
	dialogs::StartDialog *dlg = showStartDialog();
	dlg->showPage(dialogs::StartDialog::Create);
}

void MacMenu::openDocument()
{
	MainWindow *mw = dpApp().openDefault(false);
	mw->open();
}

void MacMenu::openRecent(QAction *action)
{
	QVariant filepath = action->property("filepath");
	if(filepath.isValid()) {
		MainWindow *mw = dpApp().openDefault(false);
		mw->openPath(filepath.toString());
	} else {
		dialogs::StartDialog *dlg = showStartDialog();
		dlg->showPage(dialogs::StartDialog::Recent);
	}
}

void MacMenu::startDocument()
{
	dialogs::StartDialog *dlg = showStartDialog();
	dlg->showPage(dialogs::StartDialog::Welcome);
}

void MacMenu::showSettings()
{
	MainWindow *mw = qobject_cast<MainWindow *>(qApp->activeWindow());
	if(!mw) {
		for(QWidget *widget : qApp->topLevelWidgets()) {
			mw = qobject_cast<MainWindow *>(widget);
			if(mw) {
				break;
			}
		}
	}
	if(mw) {
		mw->showSettings();
	} else {
		dialogs::StartDialog *dlg = showStartDialog();
		dlg->showPage(dialogs::StartDialog::Preferences);
	}
}

void MacMenu::hostSession()
{
	dialogs::StartDialog *dlg = showStartDialog();
	dlg->showPage(dialogs::StartDialog::Host);
}

void MacMenu::joinSession()
{
	dialogs::StartDialog *dlg = showStartDialog();
	dlg->showPage(dialogs::StartDialog::Join);
}

void MacMenu::browseSessions()
{
	dialogs::StartDialog *dlg = showStartDialog();
	dlg->showPage(dialogs::StartDialog::Browse);
}

void MacMenu::checkForUpdates()
{
	dialogs::StartDialog *dlg = showStartDialog();
	dlg->showPage(dialogs::StartDialog::Welcome);
	dlg->checkForUpdates();
}

dialogs::StartDialog *MacMenu::showStartDialog()
{
	MainWindow *mw = dpApp().openDefault(false);
	return mw->showStartDialog();
}

/**
 * @brief Quit program, closing all main windows
 *
 * This is currently used only on OSX because of the global menu bar.
 * On other platforms, there may be windows belonging to different processes
 * open, so shutting down the whole process when Quit was chosen from one window
 * may result in inconsistent operation.
 */
void MacMenu::quitAll()
{
	int mainwindows = 0;
	int dirty = 0;
	bool forceDiscard = false;

	for(const QWidget *widget : qApp->topLevelWidgets()) {
		const MainWindow *mw = qobject_cast<const MainWindow *>(widget);
		if(mw) {
			++mainwindows;
			if(!mw->canReplace())
				++dirty;
		}
	}

	if(mainwindows == 0) {
		qApp->quit();
		return;
	}

	if(dirty > 1) {
		QMessageBox box;
		box.setText(
			tr("You have %n images with unsaved changes. Do you want to review "
			   "these changes before quitting?",
			   "", dirty));
		box.setInformativeText(tr(
			"If you don't review your documents, all changes will be lost."));
		box.addButton(tr("Review changes…"), QMessageBox::AcceptRole);
		box.addButton(QMessageBox::Cancel);
		box.addButton(tr("Discard changes"), QMessageBox::DestructiveRole);

		int result = box.exec();
		if(result == QMessageBox::Cancel) {
			return;
		} else if(result == 1) {
			forceDiscard = true;
		}
	}

	qApp->setQuitOnLastWindowClosed(true);

	if(forceDiscard) {
		for(QWidget *widget : qApp->topLevelWidgets()) {
			MainWindow *mw = qobject_cast<MainWindow *>(widget);
			if(mw) {
				mw->exit();
			}
		}

	} else {
		qApp->closeAllWindows();
		bool allClosed = true;
		for(QWidget *widget : qApp->topLevelWidgets()) {
			MainWindow *mw = qobject_cast<MainWindow *>(widget);
			if(mw) {
				allClosed = false;
				break;
			}
		}
		if(!allClosed) {
			// user cancelled quit
			qApp->setQuitOnLastWindowClosed(false);
		}
	}
}

void MacMenu::winMinimize()
{
	MainWindow *w = qobject_cast<MainWindow *>(qApp->activeWindow());
	if(w) {
		w->showMinimized();
	}
}

static QString menuWinTitle(QString title)
{
	title.replace(QStringLiteral("[*]"), QString());
	return title.trimmed();
}

void MacMenu::addWindow(MainWindow *win)
{
	QAction *a = new QAction(menuWinTitle(win->windowTitle()), this);
	a->setProperty("mainwin", QVariant::fromValue(win));
	a->setCheckable(true);
	m_windows->addAction(a);
}

void MacMenu::updateWindow(MainWindow *win)
{
	QListIterator<QAction *> i(m_windows->actions());
	i.toBack();
	while(i.hasPrevious()) {
		QAction *a = i.previous();
		if(a->isSeparator()) {
			break;
		}

		if(a->property("mainwin").value<MainWindow *>() == win) {
			a->setText(menuWinTitle(win->windowTitle()));
			break;
		}
	}
}

void MacMenu::removeWindow(MainWindow *win)
{
	QListIterator<QAction *> i(m_windows->actions());
	i.toBack();
	QAction *delthis = nullptr;
	while(i.hasPrevious()) {
		QAction *a = i.previous();
		if(a->isSeparator()) {
			break;
		}

		if(a->property("mainwin").value<MainWindow *>() == win) {
			delthis = a;
			break;
		}
	}

	Q_ASSERT(delthis);
	delete delthis;
}

void MacMenu::winSelect(QAction *a)
{
	QVariant mw = a->property("mainwin");
	if(mw.isValid()) {
		MainWindow *w = mw.value<MainWindow *>();
		w->showNormal();
		w->raise();
		w->activateWindow();
	}
}

void MacMenu::updateWinMenu()
{
	const MainWindow *top = qobject_cast<MainWindow *>(qApp->activeWindow());

	QListIterator<QAction *> i(m_windows->actions());
	i.toBack();

	while(i.hasPrevious()) {
		QAction *a = i.previous();
		if(a->isSeparator()) {
			break;
		}

		// TODO show bullet if window has unsaved changes and diamond
		// if minimized.
		a->setChecked(a->property("mainwin").value<MainWindow *>() == top);
	}
}
