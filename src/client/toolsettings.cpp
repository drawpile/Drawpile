/*
	DrawPile - a collaborative drawing program.

	Copyright (C) 2006 Calle Laakkonen

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
#include <QSettings>

#include "main.h"
#include "toolsettings.h"
#include "brushpreview.h"
using widgets::BrushPreview; // qt designer doesn't know about namespaces
#include "ui_brushsettings.h"
#include "ui_simplesettings.h"

namespace tools {

ToolSettings::~ToolSettings() { /* abstract */ }

BrushSettings::BrushSettings(QString name, QString title, bool swapcolors)
	: ToolSettings(name,title), swapcolors_(swapcolors)
{
	ui_ = new Ui_BrushSettings();
}

BrushSettings::~BrushSettings()
{
	if(ui_) {
		// Remember settings
		QSettings& cfg = DrawPileApp::getSettings();
		cfg.beginGroup("tools");
		cfg.beginGroup(getName());
		cfg.setValue("size", ui_->brushsize->value());
		cfg.setValue("opacity", ui_->brushopacity->value());
		cfg.setValue("hardness", ui_->brushhardness->value());
		cfg.setValue("spacing", ui_->brushspacing->value());
		cfg.setValue("pressuresize", ui_->pressuresize->isChecked());
		cfg.setValue("pressureopacity", ui_->pressureopacity->isChecked());
		cfg.setValue("pressurehardness", ui_->pressurehardness->isChecked());
		cfg.setValue("pressurecolor", ui_->pressurecolor->isChecked());
		delete ui_;
	}
}

QWidget *BrushSettings::createUi(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	ui_->setupUi(widget);
	widget->hide();
	setUiWidget(widget);

	// Load previous settings
	QSettings& cfg = DrawPileApp::getSettings();
	cfg.beginGroup("tools");
	cfg.beginGroup(getName());
	ui_->brushsize->setValue(cfg.value("size", 0).toInt());
	ui_->brushsizebox->setValue(ui_->brushsize->value());
	ui_->preview->setSize(ui_->brushsize->value());

	ui_->brushopacity->setValue(cfg.value("opacity", 100).toInt());
	ui_->brushopacitybox->setValue(ui_->brushopacity->value());
	ui_->preview->setOpacity(ui_->brushopacity->value());

	ui_->brushhardness->setValue(cfg.value("hardness", 50).toInt());
	ui_->brushhardnessbox->setValue(ui_->brushhardness->value());
	ui_->preview->setHardness(ui_->brushhardness->value());

	ui_->brushspacing->setValue(cfg.value("spacing", 15).toInt());
	ui_->brushspacingbox->setValue(ui_->brushspacing->value());
	ui_->preview->setSpacing(ui_->brushspacing->value());

	ui_->pressuresize->setChecked(cfg.value("pressuresize",false).toBool());
	ui_->preview->setSizePressure(ui_->pressuresize->isChecked());

	ui_->pressureopacity->setChecked(cfg.value("pressureopacity",false).toBool());
	ui_->preview->setOpacityPressure(ui_->pressureopacity->isChecked());

	ui_->pressurehardness->setChecked(cfg.value("pressurehardness",false).toBool());
	ui_->preview->setHardnessPressure(ui_->pressurehardness->isChecked());

	ui_->pressurecolor->setChecked(cfg.value("pressurecolor",false).toBool());
	ui_->preview->setColorPressure(ui_->pressurecolor->isChecked());

	// Connect size change signal
	parent->connect(ui_->brushsize, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	return widget;
}

void BrushSettings::setForeground(const QColor& color)
{
	if(swapcolors_)
		ui_->preview->setColor2(color);
	else
		ui_->preview->setColor1(color);
}

void BrushSettings::setBackground(const QColor& color)
{
	if(swapcolors_)
		ui_->preview->setColor1(color);
	else
		ui_->preview->setColor2(color);
}

const drawingboard::Brush& BrushSettings::getBrush() const
{
	return ui_->preview->brush();
}

int BrushSettings::getSize() const
{
	return ui_->brushsize->value();
}

SimpleSettings::SimpleSettings(QString name, QString title, Type type)
	: ToolSettings(name,title), type_(type)
{
	ui_ = new Ui_SimpleSettings();
}

SimpleSettings::~SimpleSettings()
{
	if(ui_) {
		// Remember settings
		QSettings& cfg = DrawPileApp::getSettings();
		cfg.beginGroup("tools");
		cfg.beginGroup(getName());
		cfg.setValue("size", ui_->brushsize->value());
		cfg.setValue("opacity", ui_->brushopacity->value());
		cfg.setValue("hardness", ui_->brushhardness->value());
		cfg.setValue("spacing", ui_->brushspacing->value());
		delete ui_;
	}
}

QWidget *SimpleSettings::createUi(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	ui_->setupUi(widget);
	widget->hide();
	setUiWidget(widget);

	// Set proper preview shape
	if(type_==Line)
		ui_->preview->setPreviewShape(BrushPreview::Line);
	else if(type_==Rectangle)
		ui_->preview->setPreviewShape(BrushPreview::Rectangle);

	// Load previous settings
	QSettings& cfg = DrawPileApp::getSettings();
	cfg.beginGroup("tools");
	cfg.beginGroup(getName());
	ui_->brushsize->setValue(cfg.value("size", 0).toInt());
	ui_->brushsizebox->setValue(ui_->brushsize->value());
	ui_->preview->setSize(ui_->brushsize->value());

	ui_->brushopacity->setValue(cfg.value("opacity", 100).toInt());
	ui_->brushopacitybox->setValue(ui_->brushopacity->value());
	ui_->preview->setOpacity(ui_->brushopacity->value());

	ui_->brushhardness->setValue(cfg.value("hardness", 50).toInt());
	ui_->brushhardnessbox->setValue(ui_->brushhardness->value());
	ui_->preview->setHardness(ui_->brushhardness->value());

	ui_->brushspacing->setValue(cfg.value("spacing", 15).toInt());
	ui_->brushspacingbox->setValue(ui_->brushspacing->value());
	ui_->preview->setSpacing(ui_->brushspacing->value());

	// Connect size change signal
	parent->connect(ui_->brushsize, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	return widget;
}

void SimpleSettings::setForeground(const QColor& color)
{
	ui_->preview->setColor1(color);
}

void SimpleSettings::setBackground(const QColor& color)
{
	ui_->preview->setColor2(color);
}

const drawingboard::Brush& SimpleSettings::getBrush() const
{
	return ui_->preview->brush();
}

int SimpleSettings::getSize() const
{
	return ui_->brushsize->value();
}

NoSettings::NoSettings(const QString& name, const QString& title)
	: ToolSettings(name, title)
{
}

QWidget *NoSettings::createUi(QWidget *parent)
{
	QLabel *ui = new QLabel(QApplication::tr("This tool has no settings"),
			parent);
	setUiWidget(ui);
	return ui;
}

void NoSettings::setForeground(const QColor&)
{
}

void NoSettings::setBackground(const QColor&)
{
}

const drawingboard::Brush& NoSettings::getBrush() const
{
	// return a default brush
	static drawingboard::Brush dummy(0);
	return dummy;
}

}

