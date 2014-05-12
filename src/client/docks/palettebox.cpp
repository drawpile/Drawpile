/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2014 Calle Laakkonen

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
#include "palettebox.h"

#include "widgets/palettewidget.h"
using widgets::PaletteWidget;
#include "ui_palettebox.h"

#include <QStandardPaths>
#include <QInputDialog>
#include <QMessageBox>
#include <QDebug>
#include <QSettings>
#include <QDir>

namespace docks {

namespace {
	QString paletteDirectory() {
		return QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/palettes";
	}
}

/**
 * Create a palette box dock widget.
 * @param title dock widget title
 * @param parent parent widget
 */
PaletteBox::PaletteBox(const QString& title, QWidget *parent)
	: QDockWidget(title, parent)
{
	_ui = new Ui_PaletteBox;
	QWidget *w = new QWidget(this);
	setWidget(w);
	_ui->setupUi(w);
	_ui->palettelist->setCompleter(0);

	// Load palettes
	QStringList datapaths = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
	int okpalettes=0;
	QSet<QString> palettefiles;
	for(const QString datapath : datapaths) {
		QFileInfoList pfiles = QDir(datapath + "/palettes").entryInfoList(
				QStringList("*.gpl"),
				QDir::Files|QDir::Readable
				);

		for(QFileInfo pfile : pfiles) {
			if(!palettefiles.contains(pfile.fileName())) {
				palettefiles.insert(pfile.fileName());
				Palette pal = Palette::fromFile(pfile);
				if(pal.count()>0) {
					++okpalettes;
					_palettes.append(pal);
					_ui->palettelist->addItem(pal.name());
				}
			}
		}
	}

	// Create a default palette if none were loaded
	if(okpalettes==0) {
		Palette p = Palette::makeDefaultPalette();
		_palettes.append(p);
		_ui->palettelist->addItem(p.name());
		_ui->palette->setPalette(&_palettes.last());
	} else {
		// If there were palettes, remember which one was used the last time
		QSettings cfg;
		int last = cfg.value("history/lastpalette", 0).toInt();
		if(last<0 || last>= _palettes.count())
			last = 0;
		_ui->palettelist->setCurrentIndex(last);
		paletteChanged(last);
	}

	// Connect buttons and combobox
	connect(_ui->palette, SIGNAL(colorSelected(QColor)),
			this, SIGNAL(colorSelected(QColor)));

	connect(_ui->addpalette, SIGNAL(clicked()),
			this, SLOT(addPalette()));

	connect(_ui->delpalette, SIGNAL(clicked()),
			this, SLOT(deletePalette()));

	connect(_ui->palettelist, SIGNAL(currentIndexChanged(int)),
			this, SLOT(paletteChanged(int)));

	connect(_ui->palettelist, SIGNAL(editTextChanged(QString)),
			this, SLOT(nameChanged(QString)));
}

/**
 * Remember last selected palette and save those
 * palettes that have been modified.
 */
PaletteBox::~PaletteBox()
{
	QSettings cfg;
	cfg.setValue("history/lastpalette", _ui->palettelist->currentIndex());
	QString datadir = paletteDirectory();
	if(!QDir(datadir).mkpath("."))
		qWarning() << "Couldn't create directory:" << datadir;

	for(Palette &pal : _palettes) {
		if(pal.isModified())
			pal.save(QFileInfo(datadir, pal.filename()).absoluteFilePath());
	}
	delete _ui;
}

void PaletteBox::paletteChanged(int index)
{
	if(index==-1)
		_ui->palette->setPalette(0);
	else
		_ui->palette->setPalette(&_palettes[index]);
}

/**
 * The user has changed the name of a palette. Update the palette
 * and rename the file if it has one.
 */
void PaletteBox::nameChanged(const QString& name)
{
	if(name.isEmpty()==false) {
		Palette &pal = _palettes[_ui->palettelist->currentIndex()];
		// Check for name clashes
		// Rename only if name is unique
		if(isUniquePaletteName(name, _ui->palettelist->currentIndex())) {
			const QString datadir = paletteDirectory();
			QFile oldfile(QFileInfo(datadir, pal.filename()).absoluteFilePath());
			pal.setName(name);
			if(oldfile.exists())
				oldfile.rename(QFileInfo(datadir, pal.filename()).absoluteFilePath());
			_ui->palettelist->setItemText(_ui->palettelist->currentIndex(), name);
		}
	}
}

/**
 * Check if a palette name is unique.
 * @param name name to check
 * @param exclude a palette to exclude from the check
 */
bool PaletteBox::isUniquePaletteName(const QString& name, int excludeIdx) const
{
	for(int i=0;i<_palettes.size();++i) {
		if(i != excludeIdx && _palettes.at(i).name().compare(name,Qt::CaseInsensitive)==0)
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
	_palettes.append(Palette(name));
	_ui->palettelist->addItem(name);
	_ui->palettelist->setCurrentIndex(_ui->palettelist->count()-1);
	_ui->palettelist->setEnabled(true);
	_ui->delpalette->setEnabled(true);
}

void PaletteBox::deletePalette()
{
	const int index = _ui->palettelist->currentIndex();
	const int ret = QMessageBox::question(
			this,
			tr("Delete"),
			tr("Delete palette \"%1\"?").arg(_palettes.at(index).name()),
			QMessageBox::Yes|QMessageBox::No);
	if(ret == QMessageBox::Yes) {
		Palette pal = _palettes.takeAt(index);
		QFile fpal(QFileInfo(paletteDirectory(), pal.filename()).absoluteFilePath());
		if(fpal.exists())
			fpal.remove();
		_ui->palettelist->removeItem(index);
		if(_ui->palettelist->count()==0) {
			_ui->palettelist->setEnabled(false);
			_ui->delpalette->setEnabled(false);
		}
	}
}

}

