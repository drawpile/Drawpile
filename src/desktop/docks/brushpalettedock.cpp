/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017-2019 Calle Laakkonen

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

#include "docks/brushpalettedock.h"
#include "docks/utils.h"
#include "toolwidgets/brushsettings.h"
#include "brushes/brushpresetmodel.h"
#include "brushes/brush.h"
#include "utils/icon.h"

#include "ui_brushpalette.h"

#include <QMenu>
#include <QLineEdit>
#include <QMessageBox>

namespace docks {

struct BrushPalette::Private {
	Ui_BrushPalette ui;
	brushes::BrushPresetModel *presets;
	tools::BrushSettings *brushSettings;
};

BrushPalette::BrushPalette(QWidget *parent)
	: QDockWidget(parent), d(new Private)
{
	setStyleSheet(defaultDockStylesheet());

	d->presets = brushes::BrushPresetModel::getSharedInstance();

	// Create UI
	setWindowTitle(tr("Brushes"));

	QWidget *w = new QWidget(this);
	d->ui.setupUi(w);
	setWidget(w);

	// Folder selection
	d->ui.folder->setModel(d->presets);
	d->ui.folder->setCompleter(nullptr);

	connect(d->ui.folder, QOverload<int>::of(&QComboBox::currentIndexChanged),this, [this](int index) {
		const auto i = d->presets->index(index);
		d->ui.brushPaletteView->setRootIndex(i);
		// The first folder is always the built-in "Default" folder
		d->ui.folder->lineEdit()->setReadOnly(index == 0);
	});
	connect(d->ui.folder, &QComboBox::editTextChanged, this, [this](const QString &text) {
		d->ui.folder->setItemData(d->ui.folder->currentIndex(), text, Qt::EditRole);
	});

	d->ui.folder->setCurrentIndex(0);
	d->ui.folder->lineEdit()->setReadOnly(true);

	// Brush preset list
	d->ui.brushPaletteView->setModel(d->presets);
	d->ui.brushPaletteView->setRootIndex(d->presets->index(0));
	d->ui.brushPaletteView->setDragEnabled(true);
	d->ui.brushPaletteView->viewport()->setAcceptDrops(true);

	connect(d->ui.brushPaletteView, &QAbstractItemView::clicked, this, [this](const QModelIndex &index) {
		if(!d->brushSettings) {
			qWarning("BrushSettings not connected to BrushPalette");
			return;
		}

		const auto v = index.data(brushes::BrushPresetModel::BrushRole);
		if(v.isNull()) {
			qWarning("Brush was null!");
			return;
		}

		d->brushSettings->setCurrentBrush(v.value<brushes::ClassicBrush>());
	});

	// Set up the hamburger menu
	auto *hamburgerMenu = new QMenu(this);

	auto *addBrushAction = hamburgerMenu->addAction(icon::fromTheme("list-add"), tr("Add Brush"));
	auto *overwriteBrushAction = hamburgerMenu->addAction(icon::fromTheme("document-save"), tr("Overwrite Brush"));
	auto *deleteBrushAction = hamburgerMenu->addAction(icon::fromTheme("list-remove"), tr("Delete Brush"));
	hamburgerMenu->addSeparator();
	auto *addFolderAction = hamburgerMenu->addAction("New Folder");
	auto *deleteFolderAction = hamburgerMenu->addAction("Delete Folder");

	d->ui.menuButton->setMenu(hamburgerMenu);

	d->ui.brushPaletteView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(d->ui.brushPaletteView, &QWidget::customContextMenuRequested, this, [hamburgerMenu, this](const QPoint &point) {
		const auto idx = d->ui.brushPaletteView->indexAt(point);
		if(!idx.isValid()) {
			hamburgerMenu->popup(d->ui.brushPaletteView->mapToGlobal(point));
			return;
		}

		QMenu menu;
		auto *moveMenu = menu.addMenu(tr("Move to"));
		const int currentFolder = d->ui.folder->currentIndex();
		for(int i=0;i<d->presets->rowCount();++i) {
			auto *a = moveMenu->addAction(d->presets->index(i).data(Qt::DisplayRole).toString());
			a->setEnabled(i != currentFolder);
			a->setProperty("folder", i);
		}

		auto *overwriteAction = menu.addAction(tr("Overwrite brush"));
		auto *deleteAction = menu.addAction(tr("Delete brush"));

		QAction *choice = menu.exec(d->ui.brushPaletteView->mapToGlobal(point));

		if(choice == deleteAction)
			deleteBrush();
		else if(choice == overwriteAction)
			overwriteBrush();
		else if(choice != nullptr){
			Q_ASSERT(choice->property("folder").isValid());
			const int targetFolder = choice->property("folder").toInt();
			d->presets->moveBrush(idx, targetFolder);
		}
	});


	connect(addBrushAction, &QAction::triggered, this, &BrushPalette::addBrush);
	connect(overwriteBrushAction, &QAction::triggered, this, &BrushPalette::overwriteBrush);
	connect(deleteBrushAction, &QAction::triggered, this, &BrushPalette::deleteBrush);
	connect(addFolderAction, &QAction::triggered, this, &BrushPalette::addFolder);
	connect(deleteFolderAction, &QAction::triggered, this, &BrushPalette::deleteFolder);

}

BrushPalette::~BrushPalette()
{
	delete d;
}

void BrushPalette::connectBrushSettings(tools::ToolSettings *toolSettings)
{
	d->brushSettings = qobject_cast<tools::BrushSettings*>(toolSettings);
}

void BrushPalette::addBrush()
{
	if(!d->brushSettings) {
		qWarning("Cannot add preset: BrushSettings not connected to BrushPalette");
		return;
	}
	const auto folder = d->ui.brushPaletteView->rootIndex();
	d->presets->addBrush(folder.row(), d->brushSettings->currentBrush());
	d->ui.brushPaletteView->selectionModel()->select(
		d->presets->index(d->presets->rowCount(folder)-1, 0, folder),
		QItemSelectionModel::ClearAndSelect|QItemSelectionModel::Current
	);
}

void BrushPalette::overwriteBrush()
{
	if(!d->brushSettings) {
		qWarning("Cannot overwrite preset: BrushSettings not connected to BrushPalette");
		return;
	}
	auto sel = d->ui.brushPaletteView->selectionModel()->selectedIndexes();
	if(sel.isEmpty()) {
		qWarning("Cannot save brush preset: no selection!");
		return;
	}
	d->presets->setData(
		sel.first(),
		QVariant::fromValue(d->brushSettings->currentBrush()),
		brushes::BrushPresetModel::BrushRole
	);
}

void BrushPalette::deleteBrush()
{
	auto sel = d->ui.brushPaletteView->selectionModel()->selectedIndexes();
	if(sel.isEmpty()) {
		qWarning("No brush preset selection to delete!");
		return;
	}
	d->presets->removeRow(sel.first().row(), d->ui.brushPaletteView->rootIndex());
	d->ui.brushPaletteView->selectionModel()->clear();
}

void BrushPalette::addFolder()
{
	d->presets->addFolder(tr("New folder"));
	d->ui.folder->setCurrentIndex(d->presets->rowCount()-1);
}

void BrushPalette::deleteFolder()
{
	const auto idx = d->presets->index(d->ui.folder->currentIndex());
	const int brushCount = d->presets->rowCount(idx);
	if(brushCount > 0) {
		const auto btn = QMessageBox::question(
			this,
			tr("Delete Folder"),
			tr("Really delete folder \"%1\" and %n brushes?", "", brushCount).arg(idx.data().toString())
		);
		if(btn != QMessageBox::Yes)
			return;
	}
	d->presets->removeRow(idx.row());
}

}
