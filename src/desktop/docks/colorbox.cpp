/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2021 Calle Laakkonen

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
#include <QtColorWidgets/ColorWheel>
#include <QtColorWidgets/HueSlider>

#include "main.h"
#include "docks/colorbox.h"
#include "docks/titlewidget.h"
#include "utils/palettelistmodel.h"
#include "utils/palette.h"

#include "ui_colorbox.h"

#include <QSettings>
#include <QMessageBox>
#include <QMenu>
#include <QFileDialog>
#include <QButtonGroup>

using color_widgets::ColorWheel;

namespace docks {

ColorBox::ColorBox(const QString& title, QWidget *parent)
	: QDockWidget(title, parent), m_ui(new Ui_ColorBox), _updating(false)
{
	QWidget *w = new QWidget(this);
	w->resize(167, 95);
	setWidget(w);

	auto *titlebar = new TitleWidget(this);
	setTitleBarWidget(titlebar);

	auto *paletteBtn = new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft, titlebar);
	paletteBtn->setCheckable(true);
	paletteBtn->setAutoExclusive(true);
	paletteBtn->setText(tr("Palette"));

	auto *slidersBtn = new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter, titlebar);
	slidersBtn->setCheckable(true);
	slidersBtn->setAutoExclusive(true);
	slidersBtn->setText(tr("Sliders"));

	auto *wheelBtn = new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight, titlebar);
	wheelBtn->setCheckable(true);
	wheelBtn->setAutoExclusive(true);
	wheelBtn->setText(tr("Wheel"));

	titlebar->addCustomWidget(paletteBtn, false);
	titlebar->addCustomWidget(slidersBtn, false);
	titlebar->addCustomWidget(wheelBtn, false);
	titlebar->addCenteringSpacer();

	m_tabButtons = new QButtonGroup(this);
	m_tabButtons->addButton(paletteBtn, 0);
	m_tabButtons->addButton(slidersBtn, 1);
	m_tabButtons->addButton(wheelBtn, 2);
	connect(m_tabButtons, QOverload<QAbstractButton*,bool>::of(&QButtonGroup::buttonToggled), this, [this](QAbstractButton *b, bool checked) {
		if(!checked)
			return;
		const int id = m_tabButtons->id(b);
		m_ui->stackedWidget->setCurrentIndex(id);
	});

	m_ui->setupUi(w);

	QSettings cfg;

	int lastTab = cfg.value("history/lastcolortab", 0).toInt();
	switch(lastTab) {
	case 1: slidersBtn->setChecked(true); break;
	case 2: wheelBtn->setChecked(true); break;
	default: paletteBtn->setChecked(true); break;
	}

	//
	// Palette box tab
	//
	m_ui->palettelist->setCompleter(nullptr);
	m_ui->palettelist->setModel(PaletteListModel::getSharedInstance());

	connect(m_ui->palette, SIGNAL(colorSelected(QColor)), this, SIGNAL(colorChanged(QColor)));
	connect(m_ui->palette, SIGNAL(colorSelected(QColor)), this, SLOT(setColor(QColor)));

	QMenu *paletteMenu = new QMenu(this);
	paletteMenu->addAction(tr("New"), this, SLOT(addPalette()));
	paletteMenu->addAction(tr("Duplicate"), this, SLOT(copyPalette()));
	m_deletePalette = paletteMenu->addAction(tr("Delete"), this, SLOT(deletePalette()));

	paletteMenu->addSeparator();
	m_writeprotectPalette = paletteMenu->addAction(tr("Write Protect"), this, SLOT(toggleWriteProtect()));
	m_writeprotectPalette->setCheckable(true);

	paletteMenu->addSeparator();
	m_importPalette = paletteMenu->addAction(tr("Import..."), this, SLOT(importPalette()));
	m_exportPalette = paletteMenu->addAction(tr("Export..."), this, SLOT(exportPalette()));

	m_ui->paletteMenuButton->setMenu(paletteMenu);
	m_ui->paletteMenuButton->setStyleSheet("QToolButton::menu-indicator { image: none }");

	connect(m_ui->palettelist, SIGNAL(currentIndexChanged(int)), this, SLOT(paletteChanged(int)));
	connect(m_ui->palettelist, SIGNAL(editTextChanged(QString)), this, SLOT(paletteNameChanged(QString)));

	// Restore last used palette
	int lastPalette = cfg.value("history/lastpalette", 0).toInt();
	lastPalette = qBound(0, lastPalette, m_ui->palettelist->model()->rowCount());

	if(lastPalette>0)
		m_ui->palettelist->setCurrentIndex(lastPalette);
	else
		paletteChanged(0);

	//
	// Color slider tab
	//
	connect(m_ui->hue, SIGNAL(valueChanged(int)), this, SLOT(updateFromHsvSliders()));
	connect(m_ui->saturation, SIGNAL(valueChanged(int)), this, SLOT(updateFromHsvSliders()));
	connect(m_ui->value, SIGNAL(valueChanged(int)), this, SLOT(updateFromHsvSliders()));

	connect(m_ui->huebox, SIGNAL(valueChanged(int)), this, SLOT(updateFromHsvSpinbox()));
	connect(m_ui->saturationbox, SIGNAL(valueChanged(int)), this, SLOT(updateFromHsvSpinbox()));
	connect(m_ui->valuebox, SIGNAL(valueChanged(int)), this, SLOT(updateFromHsvSpinbox()));

	connect(m_ui->red, SIGNAL(valueChanged(int)), this, SLOT(updateFromRgbSliders()));
	connect(m_ui->green, SIGNAL(valueChanged(int)), this, SLOT(updateFromRgbSliders()));
	connect(m_ui->blue, SIGNAL(valueChanged(int)), this, SLOT(updateFromRgbSliders()));

	connect(m_ui->redbox, SIGNAL(valueChanged(int)), this, SLOT(updateFromRgbSpinbox()));
	connect(m_ui->greenbox, SIGNAL(valueChanged(int)), this, SLOT(updateFromRgbSpinbox()));
	connect(m_ui->bluebox, SIGNAL(valueChanged(int)), this, SLOT(updateFromRgbSpinbox()));

	//
	// Color wheel tab
	//
	connect(m_ui->colorwheel, SIGNAL(colorSelected(QColor)), this, SIGNAL(colorChanged(QColor)));
	connect(m_ui->colorwheel, SIGNAL(colorSelected(QColor)), this, SLOT(setColor(QColor)));

	//
	// Last used colors
	//
	m_lastused = new Palette(this);
	m_lastused->setWriteProtected(true);

	m_lastusedAlt = new Palette(this);
	m_lastusedAlt->setWriteProtected(true);

	m_ui->lastused->setPalette(m_lastused);
	m_ui->lastused->setEnableScrolling(false);
	m_ui->lastused->setMaxRows(1);

	connect(m_ui->lastused, SIGNAL(colorSelected(QColor)), this, SIGNAL(colorChanged(QColor)));
	connect(m_ui->lastused, SIGNAL(colorSelected(QColor)), this, SLOT(setColor(QColor)));

	connect(static_cast<DrawpileApp*>(qApp), &DrawpileApp::settingsChanged,
			this, &ColorBox::updateSettings);
	updateSettings();
}

ColorBox::~ColorBox()
{
	static_cast<PaletteListModel*>(m_ui->palettelist->model())->saveChanged();

	QSettings cfg;
	cfg.setValue("history/lastpalette", m_ui->palettelist->currentIndex());
	cfg.setValue("history/lastcolortab", m_ui->stackedWidget->currentIndex());
	delete m_ui;
}

void ColorBox::paletteChanged(int index)
{
	if(index<0 || index >= m_ui->palettelist->model()->rowCount()) {
		m_ui->palette->setPalette(nullptr);
	} else {
		Palette *pal = static_cast<PaletteListModel*>(m_ui->palettelist->model())->getPalette(index);
		m_ui->palette->setPalette(pal);
		m_deletePalette->setEnabled(!pal->isReadonly());
		m_writeprotectPalette->setEnabled(!pal->isReadonly());
		m_writeprotectPalette->setChecked(pal->isWriteProtected());
	}
}

void ColorBox::toggleWriteProtect()
{
	Palette *pal = m_ui->palette->palette();
	if(pal) {
		pal->setWriteProtected(!pal->isWriteProtected());
		m_writeprotectPalette->setChecked(pal->isWriteProtected());
		m_ui->palette->update();
	}
}

void ColorBox::importPalette()
{
	const QString &filename = QFileDialog::getOpenFileName(
		this,
		tr("Import palette"),
		QString(),
		tr("Palettes (%1)").arg("*.gpl") + ";;" +
		tr("All files (*)")
	);

	if(!filename.isEmpty()) {
		QScopedPointer<Palette> imported {Palette::fromFile(filename)};
		QScopedPointer<Palette> pal {Palette::copy(imported.data(), imported->name())};

		auto *palettelist = static_cast<PaletteListModel*>(m_ui->palettelist->model());
		palettelist->saveChanged();
		pal->save();
		palettelist->loadPalettes();

		m_ui->palettelist->setCurrentIndex(palettelist->findPalette(pal->name()));
	}
}

void ColorBox::exportPalette()
{
	const QString &filename = QFileDialog::getSaveFileName(
		this,
		tr("Export palette"),
		QString(),
		tr("GIMP palette (%1)").arg("*.gpl")
	);

	if(!filename.isEmpty()) {
		QString err;
		if(!m_ui->palette->palette()->exportPalette(filename, &err))
			QMessageBox::warning(this, tr("Error"), err);
	}
}

/**
 * The user has changed the name of a palette. Update the palette
 * and rename the file if it has one.
 */
void ColorBox::paletteNameChanged(const QString& name)
{
	QAbstractItemModel *m = m_ui->palettelist->model();
	m->setData(m->index(m_ui->palettelist->currentIndex(), 0), name);
}

void ColorBox::addPalette()
{
	static_cast<PaletteListModel*>(m_ui->palettelist->model())->addNewPalette();
	m_ui->palettelist->setCurrentIndex(m_ui->palettelist->count()-1);
}

void ColorBox::copyPalette()
{
	int current = m_ui->palettelist->currentIndex();
	static_cast<PaletteListModel*>(m_ui->palettelist->model())->copyPalette(current);
	m_ui->palettelist->setCurrentIndex(current);
}

void ColorBox::deletePalette()
{
	const int index = m_ui->palettelist->currentIndex();
	const int ret = QMessageBox::question(
			this,
			tr("Delete"),
			tr("Delete palette \"%1\"?").arg(m_ui->palettelist->currentText()),
			QMessageBox::Yes|QMessageBox::No);

	if(ret == QMessageBox::Yes) {
		m_ui->palettelist->model()->removeRow(index);
	}
}

void ColorBox::setColor(const QColor& color)
{
	_updating = true;
	m_ui->red->setFirstColor(QColor(0, color.green(), color.blue()));
	m_ui->red->setLastColor(QColor(255, color.green(), color.blue()));
	m_ui->red->setValue(color.red());
	m_ui->redbox->setValue(color.red());

	m_ui->green->setFirstColor(QColor(color.red(), 0, color.blue()));
	m_ui->green->setLastColor(QColor(color.red(), 255, color.blue()));
	m_ui->green->setValue(color.green());
	m_ui->greenbox->setValue(color.green());

	m_ui->blue->setFirstColor(QColor(color.red(), color.green(), 0));
	m_ui->blue->setLastColor(QColor(color.red(), color.green(), 255));
	m_ui->blue->setValue(color.blue());
	m_ui->bluebox->setValue(color.blue());


	m_ui->hue->setColorSaturation(color.saturationF());
	m_ui->hue->setColorValue(color.valueF());
	m_ui->hue->setValue(color.hue());
	m_ui->huebox->setValue(color.hue());

	m_ui->saturation->setFirstColor(QColor::fromHsv(color.hue(), 0, color.value()));
	m_ui->saturation->setLastColor(QColor::fromHsv(color.hue(), 255, color.value()));
	m_ui->saturation->setValue(color.saturation());
	m_ui->saturationbox->setValue(color.saturation());

	m_ui->value->setFirstColor(QColor::fromHsv(color.hue(), color.saturation(), 0));
	m_ui->value->setLastColor(QColor::fromHsv(color.hue(), color.saturation(), 255));
	m_ui->value->setValue(color.value());
	m_ui->valuebox->setValue(color.value());

	m_ui->colorwheel->setColor(color);
	_updating = false;
}

void ColorBox::updateFromRgbSliders()
{
	if(!_updating) {
		QColor color(m_ui->red->value(), m_ui->green->value(), m_ui->blue->value());

		setColor(color);
		emit colorChanged(color);
	}
}

void ColorBox::updateFromRgbSpinbox()
{
	if(!_updating) {
		QColor color(m_ui->redbox->value(), m_ui->greenbox->value(), m_ui->bluebox->value());
		setColor(color);
		emit colorChanged(color);
	}
}

void ColorBox::updateFromHsvSliders()
{
	if(!_updating) {
		QColor color = QColor::fromHsv(m_ui->hue->value(), m_ui->saturation->value(), m_ui->value->value());

		setColor(color);
		emit colorChanged(color);
	}
}

void ColorBox::updateFromHsvSpinbox()
{
	if(!_updating) {
		QColor color = QColor::fromHsv(m_ui->huebox->value(), m_ui->saturationbox->value(), m_ui->valuebox->value());

		setColor(color);
		emit colorChanged(color);
	}
}

void ColorBox::addLastUsedColor(const QColor &color)
{
	if(m_lastused->count()>0 && m_lastused->color(0).color.rgb() == color.rgb())
		return;

	m_lastused->setWriteProtected(false);

	// Move color to the front of the palette
	m_lastused->insertColor(0, color);
	for(int i=1;i<m_lastused->count();++i) {
		if(m_lastused->color(i).color.rgb() == color.rgb()) {
			m_lastused->removeColor(i);
			break;
		}
	}

	// Limit maximum number of remembered colors
	if(m_lastused->count() > 24)
		m_lastused->removeColor(24);
	m_lastused->setWriteProtected(true);
}

void ColorBox::swapLastUsedColors()
{
	// Swap last-used palettes
	Palette *swap = m_lastused;
	m_lastused = m_lastusedAlt;
	m_lastusedAlt = swap;
	m_ui->lastused->setPalette(m_lastused);

	// Select last used color
	const QColor altColor = m_altColor;
	m_altColor = m_ui->colorwheel->color();

	setColor(altColor);
	emit colorChanged(altColor);
}

void ColorBox::updateSettings()
{
	QSettings cfg;
	cfg.beginGroup("settings/colorwheel");
	m_ui->colorwheel->setSelectorShape(
			static_cast<ColorWheel::ShapeEnum>(cfg.value("shape").toInt()));
	m_ui->colorwheel->setRotatingSelector(
			static_cast<ColorWheel::AngleEnum>(cfg.value("rotate").toInt()));
	m_ui->colorwheel->setColorSpace(
			static_cast<ColorWheel::ColorSpaceEnum>(cfg.value("space").toInt()));
	cfg.endGroup();
}

}

