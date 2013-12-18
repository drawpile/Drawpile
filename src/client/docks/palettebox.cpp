/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <QStandardPaths>
#include <QInputDialog>
#include <QMessageBox>
#include <QDebug>
#include <QSettings>
#include <QDir>

#include "main.h"
#include "palettebox.h"

#include "widgets/palettewidget.h"
using widgets::PaletteWidget;
#include "ui_palettebox.h"

#include "utils/palette.h"

namespace widgets {

/**
 * Create a palette box dock widget.
 * @param title dock widget title
 * @param parent parent widget
 */
PaletteBox::PaletteBox(const QString& title, QWidget *parent)
	: QDockWidget(title, parent)
{
	ui_ = new Ui_PaletteBox;
	QWidget *w = new QWidget(this);
	setWidget(w);
	ui_->setupUi(w);
	ui_->palettelist->setCompleter(0);

	// Load palettes
	QStringList datapaths = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
	int okpalettes=0;
	QSet<QString> palettefiles;
	foreach(const QString datapath, datapaths) {
		QFileInfoList pfiles = QDir(datapath).entryInfoList(
				QStringList("*.gpl"),
				QDir::Files|QDir::Readable
				);

		foreach(QFileInfo pfile, pfiles) {
			if(!palettefiles.contains(pfile.fileName())) {
				palettefiles.insert(pfile.fileName());
				Palette *pal = Palette::fromFile(pfile);
				if(pal) {
					++okpalettes;
					palettes_.append(pal);
					ui_->palettelist->addItem(pal->name());
				}
			}
		}
	}

	// Create a default palette if none were loaded
	if(okpalettes==0) {
		Palette *p = Palette::makeDefaultPalette();
		palettes_.append(p);
		ui_->palettelist->addItem(p->name());
		ui_->palette->setPalette(p);
	} else {
		// If there were palettes, remember which one was used the last time
		int last = DrawPileApp::getSettings().value("history/lastpalette", 0).toInt();
		if(last<0 || last>= palettes_.count())
			last = 0;
		ui_->palettelist->setCurrentIndex(last);
		ui_->palette->setPalette(palettes_.at(last));
	}

	// Connect buttons and combobox
	connect(ui_->palette, SIGNAL(colorSelected(QColor)),
			this, SIGNAL(colorSelected(QColor)));

	connect(ui_->addpalette, SIGNAL(clicked()),
			this, SLOT(addPalette()));

	connect(ui_->delpalette, SIGNAL(clicked()),
			this, SLOT(deletePalette()));

	connect(ui_->palettelist, SIGNAL(currentIndexChanged(int)),
			this, SLOT(paletteChanged(int)));

	connect(ui_->palettelist, SIGNAL(editTextChanged(QString)),
			this, SLOT(nameChanged(QString)));
}

/**
 * Remember last selected palette and save those
 * palettes that have been modified.
 */
PaletteBox::~PaletteBox()
{
	DrawPileApp::getSettings().setValue("history/lastpalette",
			ui_->palettelist->currentIndex());
	QString datadir = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
	while(palettes_.isEmpty()==false) {
		Palette *pal = palettes_.takeFirst();
		if(pal->isModified()) {
			if(!QDir(datadir).mkpath("."))
				qWarning() << "Couldn't create directory:" << datadir;
			pal->save(QFileInfo(datadir, pal->filename()).absoluteFilePath());
		}
		delete pal;
	}
	delete ui_;
}

void PaletteBox::paletteChanged(int index)
{
	if(index==-1)
		ui_->palette->setPalette(0);
	else
		ui_->palette->setPalette(palettes_.at(index));
}

/**
 * The user has changed the name of a palette. Update the palette
 * and rename the file if it has one.
 */
void PaletteBox::nameChanged(const QString& name)
{
	if(name.isEmpty()==false) {
		Palette *pal = palettes_.at(ui_->palettelist->currentIndex());
		// Check for name clashes
		// Rename only if name is unique
		if(isUniquePaletteName(name, pal)) {
			QString datadir = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
			QFile oldfile(QFileInfo(datadir, pal->filename()).absoluteFilePath());
			pal->setName(name);
			if(oldfile.exists())
				oldfile.rename(QFileInfo(datadir, pal->filename()).absoluteFilePath());
			ui_->palettelist->setItemText(ui_->palettelist->currentIndex(), name);
		}
	}
}

/**
 * Check if a palette name is unique.
 * @param name name to check
 * @param exclude a palette to exclude from the check
 */
bool PaletteBox::isUniquePaletteName(const QString& name, const Palette *exclude) const
{
	foreach(Palette *p, palettes_) {
		if(p != exclude && p->name().compare(name,Qt::CaseInsensitive)==0)
			return false;
	}
	return true;
}

void PaletteBox::addPalette()
{

	QString name;
	do {
		name = QInputDialog::getText(this, tr("Add new palette"),
				QString("Name of the palette"));
		if(name.isEmpty())
			return;
		if(isUniquePaletteName(name,0)==false) {
			QMessageBox::information(this,tr("Name already in use"),
					tr("The palette name must be unique"));
		} else {
			break;
		}
	} while(1);

	// TODO check that name is unique
	Palette *pal = new Palette(name);
	palettes_.append(pal);
	ui_->palettelist->addItem(name);
	ui_->palettelist->setCurrentIndex(ui_->palettelist->count()-1);
	ui_->palettelist->setEnabled(true);
	ui_->delpalette->setEnabled(true);
}

void PaletteBox::deletePalette()
{
	const int index = ui_->palettelist->currentIndex();
	const int ret = QMessageBox::question(
			this,
			tr("DrawPile"),
			tr("Delete palette \"%1\"?").arg(palettes_.at(index)->name()),
			QMessageBox::Yes|QMessageBox::No);
	if(ret == QMessageBox::Yes) {
		Palette *pal = palettes_.takeAt(index);
		QFile fpal(QFileInfo(QStandardPaths::writableLocation(QStandardPaths::DataLocation), pal->filename()).absoluteFilePath());
		if(fpal.exists())
			fpal.remove();
		delete pal;
		ui_->palettelist->removeItem(index);
		if(ui_->palettelist->count()==0) {
			ui_->palettelist->setEnabled(false);
			ui_->delpalette->setEnabled(false);
		}
	}
}

}

