/*
   Drawpile - a collaborative drawing program.

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
#include "utils/palettelistmodel.h"

#include "widgets/palettewidget.h"
using widgets::PaletteWidget;
#include "ui_palettebox.h"

#include <QStandardPaths>
#include <QMessageBox>
#include <QDebug>
#include <QSettings>
#include <QDir>

namespace docks {

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
	_ui->palettelist->setCompleter(nullptr);

	_ui->palettelist->setModel(PaletteListModel::getSharedInstance());

	// Connect buttons and combobox
	connect(_ui->palette, SIGNAL(colorSelected(QColor)),
			this, SIGNAL(colorSelected(QColor)));

	connect(_ui->addpalette, SIGNAL(clicked()),
			this, SLOT(addPalette()));

	connect(_ui->copyPalette, SIGNAL(clicked()),
			this, SLOT(copyPalette()));

	connect(_ui->delpalette, SIGNAL(clicked()),
			this, SLOT(deletePalette()));

	connect(_ui->palettelist, SIGNAL(currentIndexChanged(int)),
			this, SLOT(paletteChanged(int)));

	connect(_ui->palettelist, SIGNAL(editTextChanged(QString)),
			this, SLOT(nameChanged(QString)));

	// Restore last used palette
	QSettings cfg;
	int last = cfg.value("history/lastpalette", 0).toInt();
	last = qBound(0, last, _ui->palettelist->model()->rowCount());

	if(last>0)
		_ui->palettelist->setCurrentIndex(last);
	else
		paletteChanged(0);
}

/**
 * Remember last selected palette and save those
 * palettes that have been modified.
 */
PaletteBox::~PaletteBox()
{
	static_cast<PaletteListModel*>(_ui->palettelist->model())->saveChanged();

	QSettings cfg;
	cfg.setValue("history/lastpalette", _ui->palettelist->currentIndex());

	delete _ui;
}

void PaletteBox::paletteChanged(int index)
{
	if(index==-1) {
		_ui->palette->setPalette(nullptr);
	} else {
		Palette *pal = static_cast<PaletteListModel*>(_ui->palettelist->model())->getPalette(index);
		_ui->palette->setPalette(pal);
		_ui->delpalette->setEnabled(!pal->isReadonly());
		//_ui->palettelist->setEditable(!pal->isReadonly());
	}
}

/**
 * The user has changed the name of a palette. Update the palette
 * and rename the file if it has one.
 */
void PaletteBox::nameChanged(const QString& name)
{
	QAbstractItemModel *m = _ui->palettelist->model();
	m->setData(m->index(_ui->palettelist->currentIndex(), 0), name);
}

void PaletteBox::addPalette()
{
	static_cast<PaletteListModel*>(_ui->palettelist->model())->addNewPalette();
	_ui->palettelist->setCurrentIndex(_ui->palettelist->count()-1);
}

void PaletteBox::copyPalette()
{
	int current = _ui->palettelist->currentIndex();
	static_cast<PaletteListModel*>(_ui->palettelist->model())->copyPalette(current);
	_ui->palettelist->setCurrentIndex(current);
}

void PaletteBox::deletePalette()
{
	const int index = _ui->palettelist->currentIndex();
	const int ret = QMessageBox::question(
			this,
			tr("Delete"),
			tr("Delete palette \"%1\"?").arg(_ui->palettelist->currentText()),
			QMessageBox::Yes|QMessageBox::No);

	if(ret == QMessageBox::Yes) {
		_ui->palettelist->model()->removeRow(index);
	}
}

}

