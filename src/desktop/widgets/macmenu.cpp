// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/main.h"
#include "desktop/widgets/macmenu.h"
#include "desktop/mainwindow.h"
#include "desktop/utils/recents.h"
#include "desktop/dialogs/startdialog.h"
#include "desktop/utils/widgetutils.h"

#include <QAction>
#include <QMessageBox>
#include <QUrl>

MacMenu *MacMenu::instance()
{
	static MacMenu *menu;
	if(!menu)
		menu = new MacMenu;
	return menu;
}

MacMenu::MacMenu() :
	QMenuBar(nullptr)
{
	//
	// File menu
	//
	QMenu *filemenu = addMenu(MainWindow::tr("&File"));

	QAction *newdocument = makeAction(filemenu, "newdocument", MainWindow::tr("&New"), QKeySequence::New);
	QAction *open = makeAction(filemenu, "opendocument", MainWindow::tr("&Open..."), QKeySequence::Open);

	connect(newdocument, &QAction::triggered, this, &MacMenu::newDocument);
	connect(open, &QAction::triggered, this, &MacMenu::openDocument);

	_recent = filemenu->addMenu(MainWindow::tr("Open &Recent"));
	connect(_recent, &QMenu::triggered, this, &MacMenu::openRecent);

	// Relocated menu items
	QAction *quit = makeAction(filemenu, "exitprogram", MainWindow::tr("&Quit"), QKeySequence("Ctrl+Q"));
	quit->setMenuRole(QAction::QuitRole);
	connect(quit, &QAction::triggered, this, &MacMenu::quitAll);

	QAction *preferences = makeAction(filemenu, nullptr, MainWindow::tr("Prefere&nces"), QKeySequence());
	preferences->setMenuRole(QAction::PreferencesRole);
	connect(preferences, &QAction::triggered, this, &MacMenu::showSettings);

	//
	// Session menu
	//

	QMenu *sessionmenu = addMenu(MainWindow::tr("&Session"));
	QAction *host = makeAction(sessionmenu, "hostsession", MainWindow::tr("&Host..."), QKeySequence());
	QAction *join = makeAction(sessionmenu, "joinsession", MainWindow::tr("&Join..."), QKeySequence());

	host->setEnabled(false);
	connect(join, &QAction::triggered, this, &MacMenu::joinSession);

	//
	// Window menu (Mac specific)
	//
	_windows = addMenu(MainWindow::tr("Window"));
	connect(_windows, &QMenu::triggered, this, &MacMenu::winSelect);
	connect(_windows, &QMenu::aboutToShow, this, &MacMenu::updateWinMenu);

	QAction *minimize = makeAction(_windows, nullptr, tr("Minimize"), QKeySequence("ctrl+m"));

	_windows->addSeparator();

	connect(minimize, &QAction::triggered, this, &MacMenu::winMinimize);

	//
	// Help menu
	//
	QMenu *helpmenu = addMenu(MainWindow::tr("&Help"));

	QAction *homepage = makeAction(helpmenu, "dphomepage", MainWindow::tr("&Homepage"), QKeySequence());
	QAction *about = makeAction(helpmenu, "dpabout", MainWindow::tr("&About Drawpile"), QKeySequence());
	about->setMenuRole(QAction::AboutRole);
	QAction *aboutqt = makeAction(helpmenu, "aboutqt", MainWindow::tr("About &Qt"), QKeySequence());
	aboutqt->setMenuRole(QAction::AboutQtRole);

	connect(homepage, &QAction::triggered, &MainWindow::homepage);
	connect(about, &QAction::triggered, &MainWindow::about);
	connect(aboutqt, &QAction::triggered, &QApplication::aboutQt);

	dpApp().recents().bindFileMenu(_recent);
}

QAction *MacMenu::makeAction(QMenu *menu, const char *name, const QString &text, const QKeySequence &shortcut)
{
	QAction *act;
	act = new QAction(text, this);

	if(name)
		act->setObjectName(name);

	if(shortcut.isEmpty()==false)
		act->setShortcut(shortcut);

	menu->addAction(act);

	return act;
}

void MacMenu::newDocument()
{
	dialogs::StartDialog *dlg = showStartDialog();
	dlg->showPage(dialogs::StartDialog::Create);
}

void MacMenu::openDocument()
{
	MainWindow *mw = new MainWindow;
	mw->open();
}

void MacMenu::openRecent(QAction *action)
{
	QVariant filepath = action->property("filepath");
	if(filepath.isValid()) {
		MainWindow *mw = new MainWindow;
		mw->open(QUrl::fromLocalFile(filepath.toString()));
	} else {
		dialogs::StartDialog *dlg = showStartDialog();
		dlg->showPage(dialogs::StartDialog::Recent);
	}
}

void MacMenu::showSettings()
{
	MainWindow *mw = qobject_cast<MainWindow*>(qApp->activeWindow());
	if(!mw) {
		for(QWidget *widget : qApp->topLevelWidgets()) {
			mw = qobject_cast<MainWindow*>(widget);
			if(mw) {
				break;
			}
		}
	}
	if(!mw) {
		mw = new MainWindow();
	}
	mw->showSettings();
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

dialogs::StartDialog *MacMenu::showStartDialog()
{
	MainWindow *mw = new MainWindow;
	return mw->showStartDialog();
}

/**
 * @brief Quit program, closing all main windows
 *
 * This is currently used only on OSX because of the global menu bar.
 * On other platforms, there may be windows belonging to different processes open,
 * so shutting down the whole process when Quit was chosen from one window may
 * result in inconsistent operation.
 */
void MacMenu::quitAll()
{
	int mainwindows = 0;
	int dirty = 0;
	bool forceDiscard = false;

	for(const QWidget *widget : qApp->topLevelWidgets()) {
		const MainWindow *mw = qobject_cast<const MainWindow*>(widget);
		if(mw) {
			++mainwindows;
			if(!mw->canReplace())
				++dirty;
		}
	}

	if(mainwindows==0) {
		qApp->quit();
		return;
	}

	if(dirty>1) {
		QMessageBox box;
		box.setText(tr("You have %n images with unsaved changes. Do you want to review these changes before quitting?", "", dirty));
		box.setInformativeText(tr("If you don't review your documents, all changes will be lost"));
		box.addButton(tr("Review changes..."), QMessageBox::AcceptRole);
		box.addButton(QMessageBox::Cancel);
		box.addButton(tr("Discard changes"), QMessageBox::DestructiveRole);

		int r = box.exec();

		if(r == QMessageBox::Cancel)
			return;
		else if(r == 1)
			forceDiscard = true;
	}

	qApp->setQuitOnLastWindowClosed(true);

	if(forceDiscard) {
		for(QWidget *widget : qApp->topLevelWidgets()) {
			MainWindow *mw = qobject_cast<MainWindow*>(widget);
			if(mw)
				mw->exit();
		}

	} else {
		qApp->closeAllWindows();
		bool allClosed = true;
		for(QWidget *widget : qApp->topLevelWidgets()) {
			MainWindow *mw = qobject_cast<MainWindow*>(widget);
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
	MainWindow *w = qobject_cast<MainWindow*>(qApp->activeWindow());
	if(w)
		w->showMinimized();
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

	_windows->addAction(a);
}

void MacMenu::updateWindow(MainWindow *win)
{
	QListIterator<QAction*> i(_windows->actions());
	i.toBack();
	while(i.hasPrevious()) {
		QAction *a = i.previous();
		if(a->isSeparator())
			break;

		if(a->property("mainwin").value<MainWindow*>() == win) {
			a->setText(menuWinTitle(win->windowTitle()));
			break;
		}
	}
}

void MacMenu::removeWindow(MainWindow *win)
{
	QListIterator<QAction*> i(_windows->actions());
	i.toBack();
	QAction *delthis = nullptr;
	while(i.hasPrevious()) {
		QAction *a = i.previous();
		if(a->isSeparator())
			break;

		if(a->property("mainwin").value<MainWindow*>() == win) {
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
		MainWindow *w = mw.value<MainWindow*>();
		w->showNormal();
		w->raise();
		w->activateWindow();
	}
}

void MacMenu::updateWinMenu()
{
	const MainWindow *top = qobject_cast<MainWindow*>(qApp->activeWindow());

	QListIterator<QAction*> i(_windows->actions());
	i.toBack();

	while(i.hasPrevious()) {
		QAction *a = i.previous();
		if(a->isSeparator())
			break;

		// TODO show bullet if window has unsaved changes and diamond
		// if minimized.
		a->setChecked(a->property("mainwin").value<MainWindow*>() == top);
	}
}
