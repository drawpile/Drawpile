/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2015 Calle Laakkonen

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

#include "widgets/palettewidget.h"
#include "widgets/groupedtoolbutton.h"
using widgets::PaletteWidget;
using widgets::GroupedToolButton;

#include "docks/colorbox.h"
#include "docks/utils.h"
#include "utils/palettelistmodel.h"
#include "utils/palette.h"

#include "ui_colorbox.h"

#include <QSettings>
#include <QMessageBox>
#include <QMenu>

namespace docks {

ColorBox::ColorBox(const QString& title, QWidget *parent)
	: QDockWidget(title, parent), _ui(new Ui_ColorBox), _updating(false)
{
	QWidget *w = new QWidget(this);
	w->resize(167, 95);
	setWidget(w);

	QColor highlight = palette().color(QPalette::Highlight);
	QColor hover = highlight; hover.setAlphaF(0.5);

	setStyleSheet(defaultDockStylesheet() + QStringLiteral(
		"QTabBar {"
			"alignment: middle;"
		"}"
		"QTabWidget::pane {"
			"border: none;"
		"}"
		"QTabBar::tab {"
			"padding: 15px 5px 0 2px;"
			"margin: 0 2px 0 0;"
			"border: none;"
			"border-left: 3px solid transparent;"
			"background: %3;"
		"}"
		"QTabBar::tab:hover {"
			"border-left: 3px solid %2;"
		"}"
		"QTabBar::tab:selected {"
			"border-left: 3px solid %1;"
		"}"
	).arg(
		highlight.name(),
		QString("rgba(%1, %2, %3, %4)").arg(hover.red()).arg(hover.green()).arg(hover.blue()).arg(hover.alphaF()),
		palette().color(QPalette::Window).name() // this is needed OSX. TODO how to set the whole tabbar background?
	));

	_ui->setupUi(w);

	QSettings cfg;

	int lastTab = cfg.value("history/lastcolortab", 0).toInt();
	_ui->tabWidget->setCurrentIndex(lastTab);
	_ui->tabWidget->setUsesScrollButtons(false);

	//
	// Palette box tab
	//
	_ui->palettelist->setCompleter(nullptr);
	_ui->palettelist->setModel(PaletteListModel::getSharedInstance());

	connect(_ui->palette, SIGNAL(colorSelected(QColor)), this, SIGNAL(colorChanged(QColor)));
	connect(_ui->palette, SIGNAL(colorSelected(QColor)), this, SLOT(setColor(QColor)));

	QMenu *paletteMenu = new QMenu(this);
	paletteMenu->addAction(tr("New"), this, SLOT(addPalette()));
	paletteMenu->addAction(tr("Duplicate"), this, SLOT(copyPalette()));
	_deletePalette = paletteMenu->addAction(tr("Delete"), this, SLOT(deletePalette()));
	paletteMenu->addSeparator();
	_writeprotectPalette = paletteMenu->addAction(tr("Write Protect"), this, SLOT(toggleWriteProtect()));
	_writeprotectPalette->setCheckable(true);
	_ui->paletteMenuButton->setMenu(paletteMenu);
	_ui->paletteMenuButton->setStyleSheet("QToolButton::menu-indicator { image: none }");

	connect(_ui->palettelist, SIGNAL(currentIndexChanged(int)), this, SLOT(paletteChanged(int)));
	connect(_ui->palettelist, SIGNAL(editTextChanged(QString)), this, SLOT(paletteNameChanged(QString)));

	// Restore last used palette
	int lastPalette = cfg.value("history/lastpalette", 0).toInt();
	lastPalette = qBound(0, lastPalette, _ui->palettelist->model()->rowCount());

	if(lastPalette>0)
		_ui->palettelist->setCurrentIndex(lastPalette);
	else
		paletteChanged(0);

	//
	// Color slider tab
	//
	connect(_ui->hue, SIGNAL(valueChanged(int)), this, SLOT(updateFromHsvSliders()));
	connect(_ui->saturation, SIGNAL(valueChanged(int)), this, SLOT(updateFromHsvSliders()));
	connect(_ui->value, SIGNAL(valueChanged(int)), this, SLOT(updateFromHsvSliders()));

	connect(_ui->huebox, SIGNAL(valueChanged(int)), this, SLOT(updateFromHsvSpinbox()));
	connect(_ui->saturationbox, SIGNAL(valueChanged(int)), this, SLOT(updateFromHsvSpinbox()));
	connect(_ui->valuebox, SIGNAL(valueChanged(int)), this, SLOT(updateFromHsvSpinbox()));

	connect(_ui->red, SIGNAL(valueChanged(int)), this, SLOT(updateFromRgbSliders()));
	connect(_ui->green, SIGNAL(valueChanged(int)), this, SLOT(updateFromRgbSliders()));
	connect(_ui->blue, SIGNAL(valueChanged(int)), this, SLOT(updateFromRgbSliders()));

	connect(_ui->redbox, SIGNAL(valueChanged(int)), this, SLOT(updateFromRgbSpinbox()));
	connect(_ui->bluebox, SIGNAL(valueChanged(int)), this, SLOT(updateFromRgbSpinbox()));
	connect(_ui->bluebox, SIGNAL(valueChanged(int)), this, SLOT(updateFromRgbSpinbox()));

	//
	// Color wheel tab
	//
	connect(_ui->colorwheel, SIGNAL(colorSelected(QColor)), this, SIGNAL(colorChanged(QColor)));
	connect(_ui->colorwheel, SIGNAL(colorSelected(QColor)), this, SLOT(setColor(QColor)));

	//
	// Last used colors
	//
	_lastused = new Palette(this);
	_lastused->setWriteProtected(true);
	_ui->lastused->setPalette(_lastused);
	_ui->lastused->setEnableScrolling(false);
	_ui->lastused->setMaxRows(1);

	connect(_ui->lastused, SIGNAL(colorSelected(QColor)), this, SIGNAL(colorChanged(QColor)));
	connect(_ui->lastused, SIGNAL(colorSelected(QColor)), this, SLOT(setColor(QColor)));
}

ColorBox::~ColorBox()
{
	static_cast<PaletteListModel*>(_ui->palettelist->model())->saveChanged();

	QSettings cfg;
	cfg.setValue("history/lastpalette", _ui->palettelist->currentIndex());
	cfg.setValue("history/lastcolortab", _ui->tabWidget->currentIndex());
	delete _ui;
}

void ColorBox::paletteChanged(int index)
{
	if(index<0 || index >= _ui->palettelist->model()->rowCount()) {
		_ui->palette->setPalette(nullptr);
	} else {
		Palette *pal = static_cast<PaletteListModel*>(_ui->palettelist->model())->getPalette(index);
		_ui->palette->setPalette(pal);
		_deletePalette->setEnabled(!pal->isReadonly());
		_writeprotectPalette->setEnabled(!pal->isReadonly());
		_writeprotectPalette->setChecked(pal->isWriteProtected());
	}
}

void ColorBox::toggleWriteProtect()
{
	Palette *pal = _ui->palette->palette();
	if(pal) {
		pal->setWriteProtected(!pal->isWriteProtected());
		_writeprotectPalette->setChecked(pal->isWriteProtected());
		_ui->palette->update();
	}
}

/**
 * The user has changed the name of a palette. Update the palette
 * and rename the file if it has one.
 */
void ColorBox::paletteNameChanged(const QString& name)
{
	QAbstractItemModel *m = _ui->palettelist->model();
	m->setData(m->index(_ui->palettelist->currentIndex(), 0), name);
}

void ColorBox::addPalette()
{
	static_cast<PaletteListModel*>(_ui->palettelist->model())->addNewPalette();
	_ui->palettelist->setCurrentIndex(_ui->palettelist->count()-1);
}

void ColorBox::copyPalette()
{
	int current = _ui->palettelist->currentIndex();
	static_cast<PaletteListModel*>(_ui->palettelist->model())->copyPalette(current);
	_ui->palettelist->setCurrentIndex(current);
}

void ColorBox::deletePalette()
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

void ColorBox::setColor(const QColor& color)
{
	_updating = true;
	_ui->red->setFirstColor(QColor(0, color.green(), color.blue()));
	_ui->red->setLastColor(QColor(255, color.green(), color.blue()));
	_ui->red->setValue(color.red());
	_ui->redbox->setValue(color.red());

	_ui->green->setFirstColor(QColor(color.red(), 0, color.blue()));
	_ui->green->setLastColor(QColor(color.red(), 255, color.blue()));
	_ui->green->setValue(color.green());
	_ui->greenbox->setValue(color.green());

	_ui->blue->setFirstColor(QColor(color.red(), color.green(), 0));
	_ui->blue->setLastColor(QColor(color.red(), color.green(), 255));
	_ui->blue->setValue(color.blue());
	_ui->bluebox->setValue(color.blue());


	_ui->hue->setColorSaturation(color.saturationF());
	_ui->hue->setColorValue(color.valueF());
	_ui->hue->setValue(color.hue());
	_ui->huebox->setValue(color.hue());

	_ui->saturation->setFirstColor(QColor::fromHsv(color.hue(), 0, color.value()));
	_ui->saturation->setLastColor(QColor::fromHsv(color.hue(), 255, color.value()));
	_ui->saturation->setValue(color.saturation());
	_ui->saturationbox->setValue(color.saturation());

	_ui->value->setFirstColor(QColor::fromHsv(color.hue(), color.saturation(), 0));
	_ui->value->setLastColor(QColor::fromHsv(color.hue(), color.saturation(), 255));
	_ui->value->setValue(color.value());
	_ui->valuebox->setValue(color.value());

	_ui->colorwheel->setColor(color);
	_updating = false;
}

void ColorBox::updateFromRgbSliders()
{
	if(!_updating) {
		QColor color(_ui->red->value(), _ui->green->value(), _ui->blue->value());
				
		setColor(color);
		emit colorChanged(color);
	}
}

void ColorBox::updateFromRgbSpinbox()
{
	if(!_updating) {
		QColor color(_ui->redbox->value(), _ui->greenbox->value(), _ui->bluebox->value());
		setColor(color);
		emit colorChanged(color);
	}
}

void ColorBox::updateFromHsvSliders()
{
	if(!_updating) {
		QColor color = QColor::fromHsv(_ui->hue->value(), _ui->saturation->value(), _ui->value->value());

		setColor(color);
		emit colorChanged(color);
	}
}

void ColorBox::updateFromHsvSpinbox()
{
	if(!_updating) {
		QColor color = QColor::fromHsv(_ui->huebox->value(), _ui->saturationbox->value(), _ui->valuebox->value());

		setColor(color);
		emit colorChanged(color);
	}
}

void ColorBox::addLastUsedColor(const QColor &color)
{
	if(_lastused->count()>0 && _lastused->color(0).color.rgb() == color.rgb())
		return;

	_lastused->setWriteProtected(false);
	_lastused->insertColor(0, color);
	if(_lastused->count() > 24)
		_lastused->removeColor(24);
	_lastused->setWriteProtected(true);
}

}

