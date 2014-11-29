/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "main.h"
#include "macmenu.h"
#include "mainwindow.h"
#include "utils/recentfiles.h"
#include "dialogs/newdialog.h"
#include "dialogs/hostdialog.h"
#include "dialogs/joindialog.h"

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

	QAction *preferences = makeAction(filemenu, 0, MainWindow::tr("Prefere&nces"), QKeySequence());
	preferences->setMenuRole(QAction::PreferencesRole);
	connect(preferences, &QAction::triggered, &MainWindow::showSettings);

	//
	// Session menu
	//

	QMenu *sessionmenu = addMenu(MainWindow::tr("&Session"));
	QAction *host = makeAction(sessionmenu, "hostsession", MainWindow::tr("&Host..."), QKeySequence());
	QAction *join = makeAction(sessionmenu, "joinsession", MainWindow::tr("&Join..."), QKeySequence());

	connect(host, &QAction::triggered, this, &MacMenu::hostSession);
	connect(join, &QAction::triggered, this, &MacMenu::joinSession);

	sessionmenu->addAction(host);
	sessionmenu->addAction(join);

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

	helpmenu->addAction(homepage);
	helpmenu->addSeparator();
	helpmenu->addAction(about);
	helpmenu->addAction(aboutqt);

	//
	// Initialize
	//
	updateRecentMenu();
}

void MacMenu::updateRecentMenu()
{
	RecentFiles::initMenu(_recent);
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
	auto dlg = new dialogs::NewDialog;
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	connect(dlg, &dialogs::NewDialog::accepted, [](const QSize &size, const QColor &color) {
		MainWindow *mw = new MainWindow;
		mw->newDocument(size, color);
	});

	dlg->show();
}

void MacMenu::openDocument()
{
	MainWindow *mw = new MainWindow;
	mw->open();
}

void MacMenu::openRecent(QAction *action)
{
	MainWindow *mw = new MainWindow;
	mw->open(QUrl::fromLocalFile(action->property("filepath").toString()));
}

void MacMenu::hostSession()
{
	auto dlg = new dialogs::HostDialog(QImage());
	connect(dlg, &dialogs::HostDialog::finished, [dlg](int i) {
		if(i==QDialog::Accepted) {
			dlg->rememberSettings();
			MainWindow *mw = new MainWindow;
			mw->hostSession(dlg);
		}
		dlg->deleteLater();
	});
	dlg->show();
}

void MacMenu::joinSession()
{
	auto dlg = new dialogs::JoinDialog;
	connect(dlg, &dialogs::JoinDialog::finished, [this, dlg](int i) {
		if(i==QDialog::Accepted) {
			QUrl url = dlg->getUrl();

			if(!url.isValid()) {
				// TODO add validator to prevent this from happening
				QMessageBox::warning(0, "Error", "Invalid address");
				return;
			}

			dlg->rememberSettings();

			MainWindow *mw = new MainWindow;
			mw->joinSession(url, dlg->recordSession());
		}
		dlg->deleteLater();
	});
	dlg->show();}

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
