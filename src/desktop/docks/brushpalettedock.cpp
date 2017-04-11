/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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
#include "tools/toolproperties.h"

// Work around lack of namespace support in Qt designer (TODO is the problem in our plugin?)
#include "widgets/groupedtoolbutton.h"
using widgets::GroupedToolButton;


#include "ui_brushpalette.h"

namespace docks {

struct BrushPalette::Private {
	Ui_BrushPalette ui;
	tools::BrushPresetModel *presets;
	tools::BrushSettings *brushSettings;
};

BrushPalette::BrushPalette(QWidget *parent)
	: QDockWidget(parent), d(new Private)
{
	setStyleSheet(defaultDockStylesheet());

	d->presets = tools::BrushPresetModel::getSharedInstance();

	// Create UI
	setWindowTitle(tr("Brushes"));

	QWidget *w = new QWidget(this);
	d->ui.setupUi(w);
	setWidget(w);

	// Set up preset view
	d->ui.brushPaletteView->setModel(d->presets);
	d->ui.brushPaletteView->setDragEnabled(true);
	d->ui.brushPaletteView->viewport()->setAcceptDrops(true);
	connect(d->ui.brushPaletteView, &QAbstractItemView::clicked, this, [this](const QModelIndex &index) {
		if(!d->brushSettings) {
			qWarning("BrushSettings not connected to BrushPalette");
			return;
		}

		QVariant v = index.data(tools::BrushPresetModel::ToolPropertiesRole);
		if(v.isNull()) {
			qWarning("Brush preset was null!");
			return;
		}

		d->brushSettings->setCurrentBrushSettings(tools::ToolProperties::fromVariant(v.toHash()));
	});

	connect(d->ui.presetAdd, &QAbstractButton::clicked, this, [this]() {
		if(!d->brushSettings) {
			qWarning("Cannot add preset: BrushSettings not connected to BrushPalette");
			return;
		}
		d->presets->addBrush(d->brushSettings->getCurrentBrushSettings());
	});

	connect(d->ui.presetSave, &QAbstractButton::clicked, this, [this]() {
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
			d->brushSettings->getCurrentBrushSettings().asVariant(),
			tools::BrushPresetModel::ToolPropertiesRole
		);
	});

	connect(d->ui.presetDelete, &QAbstractButton::clicked, this, [this]() {
		auto sel = d->ui.brushPaletteView->selectionModel()->selectedIndexes();
		if(sel.isEmpty()) {
			qWarning("No brush preset selection to delete!");
			return;
		}
		d->presets->removeRow(sel.first().row());
	});
}

void BrushPalette::connectBrushSettings(tools::ToolSettings *toolSettings)
{
	d->brushSettings = qobject_cast<tools::BrushSettings*>(toolSettings);
}

BrushPalette::~BrushPalette()
{
	delete d;
}

}
