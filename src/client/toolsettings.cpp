/*
	DrawPile - a collaborative drawing program.

	Copyright (C) 2006-2008 Calle Laakkonen

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
#include "colorbutton.h"
using widgets::BrushPreview; // qt designer doesn't know about namespaces
using widgets::ColorButton;
#include "ui_pensettings.h"
#include "ui_brushsettings.h"
#include "ui_simplesettings.h"
#include "ui_textsettings.h"

#include "boardeditor.h"
#include "annotationitem.h"
#include "../shared/net/annotation.h"
#include "core/layer.h"
#include "core/rasterop.h"

namespace tools {

QSettings& ToolSettings::getSettings()
{
	QSettings& cfg = DrawPileApp::getSettings();
	cfg.beginGroup("tools");
	cfg.beginGroup(name_);
	return cfg;
}

PenSettings::PenSettings(QString name, QString title)
	: ToolSettings(name, title)
{
	ui_ = new Ui_PenSettings();
}

PenSettings::~PenSettings()
{
	if(ui_) {
		// Remember settings
		QSettings& cfg = getSettings();
		cfg.setValue("size", ui_->brushsize->value());
		cfg.setValue("opacity", ui_->brushopacity->value());
		cfg.setValue("spacing", ui_->brushspacing->value());
		cfg.setValue("pressuresize", ui_->pressuresize->isChecked());
		cfg.setValue("pressureopacity", ui_->pressureopacity->isChecked());
		cfg.setValue("pressurecolor", ui_->pressurecolor->isChecked());
		delete ui_;
	}
}

QWidget *PenSettings::createUi(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	ui_->setupUi(widget);
	widget->hide();
	setUiWidget(widget);

	// Load previous settings
	QSettings& cfg = getSettings();
	ui_->brushsize->setValue(cfg.value("size", 0).toInt());
	ui_->brushsizebox->setValue(ui_->brushsize->value());
	ui_->preview->setSize(ui_->brushsize->value());

	ui_->brushopacity->setValue(cfg.value("opacity", 100).toInt());
	ui_->brushopacitybox->setValue(ui_->brushopacity->value());
	ui_->preview->setOpacity(ui_->brushopacity->value());

	ui_->preview->setHardness(100);

	ui_->brushspacing->setValue(cfg.value("spacing", 15).toInt());
	ui_->brushspacingbox->setValue(ui_->brushspacing->value());
	ui_->preview->setSpacing(ui_->brushspacing->value());

	ui_->pressuresize->setChecked(cfg.value("pressuresize",false).toBool());
	ui_->preview->setSizePressure(ui_->pressuresize->isChecked());

	ui_->pressureopacity->setChecked(cfg.value("pressureopacity",false).toBool());
	ui_->preview->setOpacityPressure(ui_->pressureopacity->isChecked());

	ui_->pressurecolor->setChecked(cfg.value("pressurecolor",false).toBool());
	ui_->preview->setColorPressure(ui_->pressurecolor->isChecked());

	ui_->preview->setSubPixel(false);

	// Connect size change signal
	parent->connect(ui_->brushsize, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	return widget;
}

void PenSettings::setForeground(const QColor& color)
{
	ui_->preview->setColor1(color);
}

void PenSettings::setBackground(const QColor& color)
{
	ui_->preview->setColor2(color);
}

const dpcore::Brush& PenSettings::getBrush() const
{
	return ui_->preview->brush();
}

int PenSettings::getSize() const
{
	return ui_->brushsize->value();
}

BrushSettings::BrushSettings(QString name, QString title, bool swapcolors)
	: ToolSettings(name,title), swapcolors_(swapcolors)
{
	ui_ = new Ui_BrushSettings();
}

BrushSettings::~BrushSettings()
{
	if(ui_) {
		// Remember settings
		QSettings& cfg = getSettings();
		cfg.setValue("blendmode", ui_->blendmode->currentIndex());
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

	// Populate blend mode combobox
	for(int b=0;b<dpcore::BLEND_MODES;++b) {
		ui_->blendmode->addItem(QApplication::tr(dpcore::BLEND_MODE[b]));
	}

	// Load previous settings
	QSettings& cfg = getSettings();

	ui_->blendmode->setCurrentIndex(cfg.value("blendmode", 0).toInt());

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

const dpcore::Brush& BrushSettings::getBrush() const
{
	return ui_->preview->brush();
}

int BrushSettings::getSize() const
{
	return ui_->brushsize->value();
}

SimpleSettings::SimpleSettings(QString name, QString title, Type type, bool sp)
	: ToolSettings(name,title), type_(type), subpixel_(sp)
{
	ui_ = new Ui_SimpleSettings();
}

SimpleSettings::~SimpleSettings()
{
	if(ui_) {
		// Remember settings
		QSettings& cfg = getSettings();
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
	ui_->preview->setSubPixel(subpixel_);

	// Load previous settings
	QSettings& cfg = getSettings();
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

const dpcore::Brush& SimpleSettings::getBrush() const
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

NoSettings::~NoSettings()
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

const dpcore::Brush& NoSettings::getBrush() const
{
	// return a default brush
	static dpcore::Brush dummy(0);
	return dummy;
}

AnnotationSettings::AnnotationSettings(QString name, QString title)
	: QObject(), ToolSettings(name, title), brush_(0), sel_(0), noupdate_(false)
{
	ui_ = new Ui_TextSettings;
}

AnnotationSettings::~AnnotationSettings()
{
	delete ui_;
}

QWidget *AnnotationSettings::createUi(QWidget *parent)
{
	uiwidget_ = new QWidget(parent);
	ui_->setupUi(uiwidget_);
	setUiWidget(uiwidget_);
	uiwidget_->setEnabled(false);

	connect(ui_->content, SIGNAL(textChanged()), this, SLOT(applyChanges()));
	connect(ui_->left, SIGNAL(clicked()), this, SLOT(applyChanges()));
	connect(ui_->center, SIGNAL(clicked()), this, SLOT(applyChanges()));
	connect(ui_->fill, SIGNAL(clicked()), this, SLOT(applyChanges()));
	connect(ui_->right, SIGNAL(clicked()), this, SLOT(applyChanges()));
	connect(ui_->bold, SIGNAL(clicked()), this, SLOT(applyChanges()));
	connect(ui_->italic, SIGNAL(clicked()), this, SLOT(applyChanges()));
	connect(ui_->font, SIGNAL(currentFontChanged(const QFont&)),
			this, SLOT(applyChanges()));
	connect(ui_->size, SIGNAL(valueChanged(int)), this, SLOT(applyChanges()));
	connect(ui_->btnText, SIGNAL(colorChanged(const QColor&)),
			this, SLOT(applyChanges()));
	connect(ui_->btnBackground, SIGNAL(colorChanged(const QColor&)),
			this, SLOT(applyChanges()));
	connect(ui_->btnRemove, SIGNAL(clicked()), this, SLOT(removeAnnotation()));
	connect(ui_->btnBake, SIGNAL(clicked()), this, SLOT(bake()));
	return uiwidget_;
}

void AnnotationSettings::setForeground(const QColor& color)
{
	brush_.setColor(color);
}

void AnnotationSettings::setBackground(const QColor& color)
{
	brush_.setColor2(color);
}

const dpcore::Brush& AnnotationSettings::getBrush() const
{
	return brush_;
}

void AnnotationSettings::unselect(drawingboard::AnnotationItem *item)
{
	if(sel_ == item)
		setSelection(0);
}

void AnnotationSettings::setSelection(drawingboard::AnnotationItem *item)
{
	noupdate_ = true;
	if(sel_)
		sel_->setHighlight(false);
	sel_ = item;
	uiwidget_->setEnabled(sel_!=0);
	if(item) {
		sel_->setHighlight(true);
		ui_->content->setText(item->text());
		ui_->btnText->setColor(item->textColor());
		ui_->btnBackground->setColor(item->backgroundColor());
		switch(item->justify()) {
			using protocol::Annotation;
			case Annotation::LEFT: ui_->left->setChecked(true); break;
			case Annotation::RIGHT: ui_->right->setChecked(true); break;
			case Annotation::CENTER: ui_->center->setChecked(true); break;
			case Annotation::FILL: ui_->fill->setChecked(true); break;
		}
		ui_->bold->setChecked(item->bold());
		ui_->italic->setChecked(item->italic());
		ui_->font->setCurrentFont(item->font());
		ui_->size->setValue(item->fontSize());
	}
	noupdate_ = false;
}

void AnnotationSettings::applyChanges()
{
	if(noupdate_)
		return;
	Q_ASSERT(sel_);
	protocol::Annotation a;
	a.id = sel_->id();
	a.rect = QRect(sel_->pos().toPoint(), sel_->size().toSize());
	a.text = ui_->content->toPlainText();
	a.textcolor = ui_->btnText->color().name();
	a.textalpha = ui_->btnText->color().alpha();
	a.backgroundcolor = ui_->btnBackground->color().name();
	a.bgalpha = ui_->btnBackground->color().alpha();
	if(ui_->left->isChecked())
		a.justify = protocol::Annotation::LEFT;
	else if(ui_->right->isChecked())
		a.justify = protocol::Annotation::RIGHT;
	else if(ui_->center->isChecked())
		a.justify = protocol::Annotation::CENTER;
	else if(ui_->fill->isChecked())
		a.justify = protocol::Annotation::FILL;
	a.bold = ui_->bold->isChecked();
	a.italic = ui_->italic->isChecked();
	a.font = ui_->font->currentFont().family();
	a.size = ui_->size->value();

	editor_->annotate(a);
}

void AnnotationSettings::removeAnnotation()
{
	Q_ASSERT(sel_);
	editor_->removeAnnotation(sel_->id());
}

void AnnotationSettings::bake()
{
	Q_ASSERT(sel_);
	int x, y;
	dpcore::Layer *layer = sel_->toLayer(&x, &y);
	editor_->mergeLayer(x, y, layer);
	delete layer;
	removeAnnotation();
	emit baked();
}

/**
 * Currently we can't bake annotations when in a network session,
 * because we have no way of knowing if every user has the same
 * fonts and the same font rendering engines.
 * When we can upload arbitrary raster data as drawing commands,
 * this can be removed.
 */
void AnnotationSettings::enableBaking(bool enable)
{
	ui_->btnBake->setEnabled(enable);
}

}

