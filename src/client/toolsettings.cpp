/*
	DrawPile - a collaborative drawing program.

	Copyright (C) 2006-2013 Calle Laakkonen

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
#include <QDebug>
#include <QSettings>
#include <QTimer>

#include "main.h"
#include "toolsettings.h"
#include "docks/layerlistdock.h"
#include "widgets/brushpreview.h"
#include "widgets/colorbutton.h"
#include "widgets/palettewidget.h"
using widgets::BrushPreview; // qt designer doesn't know about namespaces (TODO works in qt5?)
using widgets::ColorButton;
#include "ui_pensettings.h"
#include "ui_brushsettings.h"
#include "ui_erasersettings.h"
#include "ui_simplesettings.h"
#include "ui_textsettings.h"

#include "annotationitem.h"
#include "net/client.h"

#include "utils/palette.h"

#include "core/rasterop.h" // for blend modes


namespace {
	static dpcore::Brush DUMMY_BRUSH(0);
}

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
		cfg.setValue("blendmode", ui_->blendmode->currentIndex());
		cfg.setValue("incremental", ui_->incremental->isChecked());
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

	// Populate blend mode combobox
	// Blend mode 0 is reserved for the eraser
	for(int b=1;b<dpcore::BLEND_MODES;++b) {
		ui_->blendmode->addItem(QApplication::tr(dpcore::BLEND_MODE[b]));
	}

	// Load previous settings
	QSettings& cfg = getSettings();
	ui_->blendmode->setCurrentIndex(cfg.value("blendmode", 0).toInt());

	ui_->incremental->setChecked(cfg.value("incremental", true).toBool());
	ui_->preview->setIncremental(ui_->incremental->isChecked());

	ui_->brushsize->setValue(cfg.value("size", 0).toInt());
	ui_->preview->setSize(ui_->brushsize->value());

	ui_->brushopacity->setValue(cfg.value("opacity", 100).toInt());
	ui_->preview->setOpacity(ui_->brushopacity->value());

	ui_->preview->setHardness(100);

	ui_->brushspacing->setValue(cfg.value("spacing", 15).toInt());
	ui_->preview->setSpacing(ui_->brushspacing->value());

	ui_->pressuresize->setChecked(cfg.value("pressuresize",false).toBool());
	ui_->preview->setSizePressure(ui_->pressuresize->isChecked());

	ui_->pressureopacity->setChecked(cfg.value("pressureopacity",false).toBool());
	ui_->preview->setOpacityPressure(ui_->pressureopacity->isChecked());

	ui_->pressurecolor->setChecked(cfg.value("pressurecolor",false).toBool());
	ui_->preview->setColorPressure(ui_->pressurecolor->isChecked());

	ui_->preview->setSubpixel(false);

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

const dpcore::Brush& PenSettings::getBrush(bool swapcolors) const
{
	return ui_->preview->brush(swapcolors);
}

int PenSettings::getSize() const
{
	return ui_->brushsize->value();
}

EraserSettings::EraserSettings(QString name, QString title)
	: ToolSettings(name,title)
{
	ui_ = new Ui_EraserSettings();
}

EraserSettings::~EraserSettings()
{
	if(ui_) {
		// Remember settings
		QSettings& cfg = getSettings();
		cfg.setValue("size", ui_->brushsize->value());
		cfg.setValue("opacity", ui_->brushopacity->value());
		cfg.setValue("hardness", ui_->brushhardness->value());
		cfg.setValue("spacing", ui_->brushspacing->value());
		cfg.setValue("pressuresize", ui_->pressuresize->isChecked());
		cfg.setValue("pressureopacity", ui_->pressureopacity->isChecked());
		cfg.setValue("pressurehardness", ui_->pressurehardness->isChecked());
		cfg.setValue("hardedge", ui_->hardedge->isChecked());
		cfg.setValue("incremental", ui_->incremental->isChecked());
		delete ui_;
	}
}

QWidget *EraserSettings::createUi(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	ui_->setupUi(widget);
	widget->hide();
	setUiWidget(widget);

	parent->connect(ui_->hardedge, &QToolButton::toggled, [this](bool hard) { ui_->brushhardness->setEnabled(!hard); });
	parent->connect(ui_->brushsize, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));

	// Load previous settings
	QSettings& cfg = getSettings();

	ui_->preview->setBlendingMode(-1); // eraser is normally not visible

	ui_->brushsize->setValue(cfg.value("size", 0).toInt());
	ui_->preview->setSize(ui_->brushsize->value());

	ui_->brushopacity->setValue(cfg.value("opacity", 100).toInt());
	ui_->preview->setOpacity(ui_->brushopacity->value());

	ui_->brushhardness->setValue(cfg.value("hardness", 50).toInt());
	ui_->preview->setHardness(ui_->brushhardness->value());

	ui_->brushspacing->setValue(cfg.value("spacing", 15).toInt());
	ui_->preview->setSpacing(ui_->brushspacing->value());

	ui_->pressuresize->setChecked(cfg.value("pressuresize",false).toBool());
	ui_->preview->setSizePressure(ui_->pressuresize->isChecked());

	ui_->pressureopacity->setChecked(cfg.value("pressureopacity",false).toBool());
	ui_->preview->setOpacityPressure(ui_->pressureopacity->isChecked());

	ui_->pressurehardness->setChecked(cfg.value("pressurehardness",false).toBool());
	ui_->preview->setHardnessPressure(ui_->pressurehardness->isChecked());

	ui_->hardedge->setChecked(cfg.value("hardedge", false).toBool());

	ui_->incremental->setChecked(cfg.value("incremental", true).toBool());
	ui_->preview->setIncremental(ui_->incremental->isChecked());

	return widget;
}

void EraserSettings::setForeground(const QColor& color)
{
}

void EraserSettings::setBackground(const QColor& color)
{
	// This is used just for the background color
	ui_->preview->setColor2(color);
}

const dpcore::Brush& EraserSettings::getBrush(bool swapcolors) const
{
	return ui_->preview->brush(swapcolors);
}

int EraserSettings::getSize() const
{
	return ui_->brushsize->value();
}

BrushSettings::BrushSettings(QString name, QString title)
	: ToolSettings(name,title)
{
	ui_ = new Ui_BrushSettings();
}

BrushSettings::~BrushSettings()
{
	if(ui_) {
		// Remember settings
		QSettings& cfg = getSettings();
		cfg.setValue("blendmode", ui_->blendmode->currentIndex());
		cfg.setValue("incremental", ui_->incremental->isChecked());
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
	for(int b=1;b<dpcore::BLEND_MODES;++b) {
		ui_->blendmode->addItem(QApplication::tr(dpcore::BLEND_MODE[b]));
	}

	// Connect size change signal
	parent->connect(ui_->brushsize, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));

	// Load previous settings
	QSettings& cfg = getSettings();

	ui_->blendmode->setCurrentIndex(cfg.value("blendmode", 0).toInt());

	ui_->incremental->setChecked(cfg.value("incremental", true).toBool());
	ui_->preview->setIncremental(ui_->incremental->isChecked());

	ui_->brushsize->setValue(cfg.value("size", 0).toInt());
	ui_->preview->setSize(ui_->brushsize->value());

	ui_->brushopacity->setValue(cfg.value("opacity", 100).toInt());
	ui_->preview->setOpacity(ui_->brushopacity->value());

	ui_->brushhardness->setValue(cfg.value("hardness", 50).toInt());
	ui_->preview->setHardness(ui_->brushhardness->value());

	ui_->brushspacing->setValue(cfg.value("spacing", 15).toInt());
	ui_->preview->setSpacing(ui_->brushspacing->value());

	ui_->pressuresize->setChecked(cfg.value("pressuresize",false).toBool());
	ui_->preview->setSizePressure(ui_->pressuresize->isChecked());

	ui_->pressureopacity->setChecked(cfg.value("pressureopacity",false).toBool());
	ui_->preview->setOpacityPressure(ui_->pressureopacity->isChecked());

	ui_->pressurehardness->setChecked(cfg.value("pressurehardness",false).toBool());
	ui_->preview->setHardnessPressure(ui_->pressurehardness->isChecked());

	ui_->pressurecolor->setChecked(cfg.value("pressurecolor",false).toBool());
	ui_->preview->setColorPressure(ui_->pressurecolor->isChecked());

	return widget;
}

void BrushSettings::setForeground(const QColor& color)
{
	ui_->preview->setColor1(color);
}

void BrushSettings::setBackground(const QColor& color)
{
	ui_->preview->setColor2(color);
}

const dpcore::Brush& BrushSettings::getBrush(bool swapcolors) const
{
	return ui_->preview->brush(swapcolors);
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
		cfg.setValue("blendmode", ui_->blendmode->currentIndex());
		cfg.setValue("incremental", ui_->incremental->isChecked());
		cfg.setValue("size", ui_->brushsize->value());
		cfg.setValue("opacity", ui_->brushopacity->value());
		cfg.setValue("hardness", ui_->brushhardness->value());
		cfg.setValue("spacing", ui_->brushspacing->value());
		cfg.setValue("hardedge", ui_->hardedge->isChecked());
		delete ui_;
	}
}

QWidget *SimpleSettings::createUi(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	ui_->setupUi(widget);
	widget->hide();
	setUiWidget(widget);

	// Populate blend mode combobox
	for(int b=1;b<dpcore::BLEND_MODES;++b) {
		ui_->blendmode->addItem(QApplication::tr(dpcore::BLEND_MODE[b]));
	}

	// Connect size change signal
	parent->connect(ui_->brushsize, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	parent->connect(ui_->hardedge, &QToolButton::toggled, [this](bool hard) { ui_->brushhardness->setEnabled(!hard); });

	// Set proper preview shape
	if(type_==Line)
		ui_->preview->setPreviewShape(BrushPreview::Line);
	else if(type_==Rectangle)
		ui_->preview->setPreviewShape(BrushPreview::Rectangle);
	ui_->preview->setSubpixel(subpixel_);

	// Load previous settings
	QSettings& cfg = getSettings();
	ui_->blendmode->setCurrentIndex(cfg.value("blendmode", 0).toInt());

	ui_->incremental->setChecked(cfg.value("incremental", true).toBool());
	ui_->preview->setIncremental(ui_->incremental->isChecked());

	ui_->brushsize->setValue(cfg.value("size", 0).toInt());
	ui_->preview->setSize(ui_->brushsize->value());

	ui_->brushopacity->setValue(cfg.value("opacity", 100).toInt());
	ui_->preview->setOpacity(ui_->brushopacity->value());

	ui_->brushhardness->setValue(cfg.value("hardness", 50).toInt());
	ui_->preview->setHardness(ui_->brushhardness->value());

	ui_->brushspacing->setValue(cfg.value("spacing", 15).toInt());
	ui_->preview->setSpacing(ui_->brushspacing->value());

	ui_->hardedge->setChecked(cfg.value("hardedge", false).toBool());

	if(!subpixel_) {
		// If subpixel accuracy wasn't enabled, don't offer a chance to
		// enable it.
		ui_->hardedge->hide();
		ui_->brushopts->addSpacing(ui_->hardedge->width());
	}

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

const dpcore::Brush& SimpleSettings::getBrush(bool swapcolors) const
{
	return ui_->preview->brush(swapcolors);
}

int SimpleSettings::getSize() const
{
	return ui_->brushsize->value();
}

ColorPickerSettings::ColorPickerSettings(const QString &name, const QString &title)
	:  QObject(), ToolSettings(name, title), _palette(new Palette("Color picker"))
{
}

QWidget *ColorPickerSettings::createUi(QWidget *parent)
{
	_palettewidget = new widgets::PaletteWidget(parent);
	_palettewidget->setPalette(_palette);
	_palettewidget->setSwatchSize(32, 24);
	_palettewidget->setSpacing(3);

	connect(_palettewidget, SIGNAL(colorSelected(QColor)), this, SIGNAL(colorSelected(QColor)));

	setUiWidget(_palettewidget);
	return _palettewidget;
}

ColorPickerSettings::~ColorPickerSettings()
{
	delete _palette;
}

const dpcore::Brush &ColorPickerSettings::getBrush(bool swapcolors) const
{
	Q_UNUSED(swapcolors);
	return DUMMY_BRUSH;
}

void ColorPickerSettings::addColor(const QColor &color)
{
	if(_palette->count() && _palette->color(0) == color)
		return;

	_palette->insertColor(0, color);

	if(_palette->count() > 80)
		_palette->removeColor(_palette->count()-1);

	_palettewidget->update();
}

AnnotationSettings::AnnotationSettings(QString name, QString title)
	: QObject(), ToolSettings(name, title), noupdate_(false)
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

	_updatetimer = new QTimer(this);
	_updatetimer->setInterval(500);
	_updatetimer->setSingleShot(true);

	// Editor events
	connect(ui_->content, SIGNAL(textChanged()), this, SLOT(applyChanges()));
	connect(ui_->content, SIGNAL(cursorPositionChanged()), this, SLOT(updateStyleButtons()));

	connect(ui_->btnBackground, SIGNAL(colorChanged(const QColor&)),
			this, SLOT(applyChanges()));
	connect(ui_->btnRemove, SIGNAL(clicked()), this, SLOT(removeAnnotation()));
	connect(ui_->btnBake, SIGNAL(clicked()), this, SLOT(bake()));

	// Intra-editor connections that couldn't be made in the UI designer
	connect(ui_->left, SIGNAL(clicked()), this, SLOT(changeAlignment()));
	connect(ui_->center, SIGNAL(clicked()), this, SLOT(changeAlignment()));
	connect(ui_->justify, SIGNAL(clicked()), this, SLOT(changeAlignment()));
	connect(ui_->right, SIGNAL(clicked()), this, SLOT(changeAlignment()));
	connect(ui_->bold, SIGNAL(toggled(bool)), this, SLOT(toggleBold(bool)));

	connect(_updatetimer, SIGNAL(timeout()), this, SLOT(saveChanges()));

	return uiwidget_;
}

void AnnotationSettings::updateStyleButtons()
{
	QTextBlockFormat bf = ui_->content->textCursor().blockFormat();
	switch(bf.alignment()) {
	case Qt::AlignLeft: ui_->left->setChecked(true); break;
	case Qt::AlignCenter: ui_->center->setChecked(true); break;
	case Qt::AlignJustify: ui_->justify->setChecked(true); break;
	case Qt::AlignRight: ui_->right->setChecked(true); break;
	default: break;
	}
	QTextCharFormat cf = ui_->content->textCursor().charFormat();
	ui_->btnTextColor->setColor(cf.foreground().color());
	if(cf.fontPointSize() < 1)
		ui_->size->setValue(12);
	else
		ui_->size->setValue(cf.fontPointSize());
	ui_->font->setFont(cf.font());
	ui_->italic->setChecked(cf.fontItalic());
	ui_->bold->setChecked(cf.fontWeight() > QFont::Normal);
}

void AnnotationSettings::toggleBold(bool bold)
{
	ui_->content->setFontWeight(bold ? QFont::Bold : QFont::Normal);
}

void AnnotationSettings::changeAlignment()
{
	Qt::Alignment a = Qt::AlignLeft;
	if(ui_->left->isChecked())
		a = Qt::AlignLeft;
	else if(ui_->center->isChecked())
		a = Qt::AlignCenter;
	else if(ui_->justify->isChecked())
		a = Qt::AlignJustify;
	else if(ui_->right->isChecked())
		a = Qt::AlignRight;

	ui_->content->setAlignment(a);
}

void AnnotationSettings::setForeground(const QColor& color)
{
}

void AnnotationSettings::setBackground(const QColor& color)
{
}

const dpcore::Brush& AnnotationSettings::getBrush(bool swapcolors) const
{
	Q_UNUSED(swapcolors);
	return DUMMY_BRUSH;
}

int AnnotationSettings::selected() const
{
	if(_selection.isNull())
		return 0;
	return _selection->id();
}

void AnnotationSettings::unselect(int id)
{
	if(selected() == id)
		setSelection(0);
}

void AnnotationSettings::setSelection(drawingboard::AnnotationItem *item)
{
	noupdate_ = true;
	uiwidget_->setEnabled(item!=0);

	if(_selection)
		_selection->setHighlight(false);

	_selection = item;
	if(item) {
		item->setHighlight(true);
		ui_->content->setHtml(item->text());
		ui_->btnBackground->setColor(item->backgroundColor());
	}
	noupdate_ = false;
}

void AnnotationSettings::applyChanges()
{
	if(noupdate_)
		return;
	Q_ASSERT(selected());
	_updatetimer->start();
}

void AnnotationSettings::saveChanges()
{
	Q_ASSERT(_client);

	if(selected())
		_client->sendAnnotationEdit(
			selected(),
			ui_->btnBackground->color(),
			ui_->content->toHtml()
		);
}

void AnnotationSettings::removeAnnotation()
{
	Q_ASSERT(selected());
	Q_ASSERT(_client);
	_client->sendAnnotationDelete(selected());
	setSelection(0); /* not strictly necessary, but makes the UI seem more responsive */
}

void AnnotationSettings::bake()
{
	Q_ASSERT(selected());
	Q_ASSERT(_layerlist);
	Q_ASSERT(_client);

	QImage img = _selection->toImage();
	int layer = _layerlist->currentLayer();
	_client->sendImage(layer, _selection->pos().x(), _selection->pos().y(), img, true);
	removeAnnotation();
}

}

