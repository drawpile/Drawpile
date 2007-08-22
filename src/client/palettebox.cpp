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

#include <QInputDialog>
#include <QMessageBox>
#include <QDebug>
#include <QSettings>
#include <QDir>

#include "main.h"
#include "palettebox.h"

#include "palettewidget.h"
using widgets::PaletteWidget;
#include "ui_palettebox.h"

#include "localpalette.h"

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

	// Load palettes
	QDir confdir(DrawPileApp::getConfDir());
	QFileInfoList pfiles = confdir.entryInfoList(
			QStringList("*.gpl"),
			QDir::Files|QDir::Readable
			);

	int okpalettes=0;
	foreach(QFileInfo pfile, pfiles) {
		LocalPalette *pal = LocalPalette::fromFile(pfile);
		if(pal) {
			++okpalettes;
			palettes_.append(pal);
			ui_->palettelist->addItem(pal->name());
		}
	}

	// Create a default palette if none were loaded
	if(okpalettes==0) {
		LocalPalette * p = LocalPalette::makeDefaultPalette();
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
	QString confdir = DrawPileApp::getConfDir();
	while(palettes_.isEmpty()==false) {
		LocalPalette *pal = palettes_.takeFirst();
		if(pal->isModified())
			pal->save(QFileInfo(confdir,pal->filename()).absoluteFilePath());
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

void PaletteBox::nameChanged(const QString& name)
{
	if(name.isEmpty()==false) {
		palettes_.at(ui_->palettelist->currentIndex())->setName(name);
		ui_->palettelist->setItemText(ui_->palettelist->currentIndex(), name);
	}
}

void PaletteBox::addPalette()
{
	QString name = QInputDialog::getText(this, tr("Add new palette"),
			QString("Name of the palette"));
	if(name.isEmpty()==false) {
		LocalPalette *pal = new LocalPalette(name);
		palettes_.append(pal);
		ui_->palettelist->addItem(name);
		ui_->palettelist->setCurrentIndex(ui_->palettelist->count()-1);
		ui_->palettelist->setEnabled(true);
		ui_->delpalette->setEnabled(true);
	}
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
		delete palettes_.takeAt(index);
		ui_->palettelist->removeItem(index);
		if(ui_->palettelist->count()==0) {
			ui_->palettelist->setEnabled(false);
			ui_->delpalette->setEnabled(false);
		}
	}
}

}

