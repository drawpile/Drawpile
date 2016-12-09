/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2016 Calle Laakkonen

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

#include "toolsettings.h"
#include "toolproperties.h"
#include "docks/layerlistdock.h"
#include "widgets/brushpreview.h"
#include "widgets/colorbutton.h"
#include "widgets/palettewidget.h"
#include "widgets/groupedtoolbutton.h"
using widgets::BrushPreview; // qt designer doesn't know about namespaces (TODO works in qt5?)
using widgets::ColorButton;
using widgets::GroupedToolButton;
#include "ui_pensettings.h"
#include "ui_brushsettings.h"
#include "ui_smudgesettings.h"
#include "ui_erasersettings.h"
#include "ui_simplesettings.h"
#include "ui_textsettings.h"
#include "ui_selectsettings.h"
#include "ui_lasersettings.h"
#include "ui_fillsettings.h"

#include "scene/annotationitem.h"
#include "scene/selectionitem.h"
#include "scene/canvasview.h"
#include "scene/canvasscene.h"
#include "net/client.h"
#include "net/userlist.h"

#include "utils/palette.h"
#include "utils/icon.h"

#include "core/annotation.h"
#include "core/blendmodes.h"

#include <QTimer>
#include <QTextBlock>

namespace tools {

namespace {
	void populateBlendmodeBox(QComboBox *box, widgets::BrushPreview *preview) {
		for(auto bm : paintcore::getBlendModeNames(paintcore::BlendMode::BrushMode))
			box->addItem(bm.second, bm.first);

		preview->setBlendingMode(paintcore::BlendMode::MODE_NORMAL);
		box->connect(box, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [box,preview](int) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
			preview->setBlendingMode(paintcore::BlendMode::Mode(box->currentData().toInt()));
#else
			preview->setBlendingMode(paintcore::BlendMode::Mode(box->itemData(box->currentIndex(), Qt::UserRole).toInt()));
#endif
		});
	}
}

QWidget *ToolSettings::createUi(QWidget *parent)
{
	Q_ASSERT(_widget==0);
	_widget = createUiWidget(parent);
	return _widget;
}

ToolProperties ToolSettings::saveToolSettings()
{
	return ToolProperties();
}

void ToolSettings::restoreToolSettings(const ToolProperties &)
{
}


PenSettings::PenSettings(QString name, QString title)
	: ToolSettings(name, title, "draw-freehand"), _ui(0)
{
}

PenSettings::~PenSettings()
{
	delete _ui;
}

QWidget *PenSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	_ui = new Ui_PenSettings;
	_ui->setupUi(widget);

	populateBlendmodeBox(_ui->blendmode, _ui->preview);

	parent->connect(_ui->brushsize, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	parent->connect(_ui->preview, SIGNAL(requestFgColorChange()), parent, SLOT(changeForegroundColor()));
	parent->connect(_ui->preview, SIGNAL(requestBgColorChange()), parent, SLOT(changeBackgroundColor()));
	return widget;
}

void PenSettings::restoreToolSettings(const ToolProperties &cfg)
{
	_ui->blendmode->setCurrentIndex(cfg.intValue("blendmode", 0));

	if(cfg.boolValue("incremental", true))
		_ui->paintmodeIncremental->setChecked(true);
	else
		_ui->paintmodeIndirect->setChecked(true);

	_ui->preview->setIncremental(_ui->paintmodeIncremental->isChecked());

	_ui->brushsize->setValue(cfg.intValue("size", 0));
	_ui->preview->setSize(_ui->brushsize->value());

	_ui->brushopacity->setValue(cfg.intValue("opacity", 100));
	_ui->preview->setOpacity(_ui->brushopacity->value());

	_ui->preview->setHardness(100);

	_ui->brushspacing->setValue(cfg.intValue("spacing", 15));
	_ui->preview->setSpacing(_ui->brushspacing->value());

	_ui->pressuresize->setChecked(cfg.boolValue("pressuresize",false));
	_ui->preview->setSizePressure(_ui->pressuresize->isChecked());

	_ui->pressureopacity->setChecked(cfg.boolValue("pressureopacity",false));
	_ui->preview->setOpacityPressure(_ui->pressureopacity->isChecked());

	_ui->preview->setSubpixel(false);
}

ToolProperties PenSettings::saveToolSettings()
{
	ToolProperties cfg;
	cfg.setValue("blendmode", _ui->blendmode->currentIndex());
	cfg.setValue("incremental", _ui->paintmodeIncremental->isChecked());
	cfg.setValue("size", _ui->brushsize->value());
	cfg.setValue("opacity", _ui->brushopacity->value());
	cfg.setValue("spacing", _ui->brushspacing->value());
	cfg.setValue("pressuresize", _ui->pressuresize->isChecked());
	cfg.setValue("pressureopacity", _ui->pressureopacity->isChecked());
	return cfg;
}

void PenSettings::setForeground(const QColor& color)
{
	_ui->preview->setColor1(color);
}

void PenSettings::setBackground(const QColor& color)
{
	_ui->preview->setColor2(color);
}

void PenSettings::quickAdjust1(float adjustment)
{
	int adj = qRound(adjustment);
	if(adj!=0)
		_ui->brushsize->setValue(_ui->brushsize->value() + adj);
}

paintcore::Brush PenSettings::getBrush(bool swapcolors) const
{
	return _ui->preview->brush(swapcolors);
}

int PenSettings::getSize() const
{
	return _ui->brushsize->value();
}

EraserSettings::EraserSettings(QString name, QString title)
	: ToolSettings(name, title, "draw-eraser"), _ui(0)
{
}

EraserSettings::~EraserSettings()
{
	delete _ui;
}

QWidget *EraserSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	_ui = new Ui_EraserSettings();
	_ui->setupUi(widget);

	_ui->preview->setBlendingMode(paintcore::BlendMode::MODE_ERASE);

	parent->connect(_ui->paintmodeHardedge, &QToolButton::toggled, [this](bool hard) {
		_ui->brushhardness->setEnabled(!hard);
		_ui->spinHardness->setEnabled(!hard);
		_ui->pressurehardness->setEnabled(!hard);
	});
	parent->connect(_ui->paintmodeHardedge, SIGNAL(toggled(bool)), parent, SLOT(updateSubpixelMode()));
	parent->connect(_ui->brushsize, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	parent->connect(_ui->preview, SIGNAL(requestFgColorChange()), parent, SLOT(changeForegroundColor()));
	parent->connect(_ui->preview, SIGNAL(requestBgColorChange()), parent, SLOT(changeBackgroundColor()));

	parent->connect(_ui->colorEraseMode, &QToolButton::toggled, [this](bool color) {
		_ui->preview->setBlendingMode(color ? paintcore::BlendMode::MODE_COLORERASE : paintcore::BlendMode::MODE_ERASE);
	});

	return widget;
}

ToolProperties EraserSettings::saveToolSettings()
{
	ToolProperties cfg;
	cfg.setValue("size", _ui->brushsize->value());
	cfg.setValue("opacity", _ui->brushopacity->value());
	cfg.setValue("hardness", _ui->brushhardness->value());
	cfg.setValue("spacing", _ui->brushspacing->value());
	cfg.setValue("pressuresize", _ui->pressuresize->isChecked());
	cfg.setValue("pressureopacity", _ui->pressureopacity->isChecked());
	cfg.setValue("pressurehardness", _ui->pressurehardness->isChecked());
	cfg.setValue("hardedge", _ui->paintmodeHardedge->isChecked());
	cfg.setValue("incremental", _ui->paintmodeIncremental->isChecked());
	cfg.setValue("colorerase", _ui->colorEraseMode->isChecked());
	return cfg;
}

void EraserSettings::restoreToolSettings(const ToolProperties &cfg)
{
	_ui->brushsize->setValue(cfg.intValue("size", 10));
	_ui->preview->setSize(_ui->brushsize->value());

	_ui->brushopacity->setValue(cfg.intValue("opacity", 100));
	_ui->preview->setOpacity(_ui->brushopacity->value());

	_ui->brushhardness->setValue(cfg.intValue("hardness", 50));
	_ui->preview->setHardness(_ui->brushhardness->value());

	_ui->brushspacing->setValue(cfg.intValue("spacing", 15));
	_ui->preview->setSpacing(_ui->brushspacing->value());

	_ui->pressuresize->setChecked(cfg.boolValue("pressuresize",false));
	_ui->preview->setSizePressure(_ui->pressuresize->isChecked());

	_ui->pressureopacity->setChecked(cfg.boolValue("pressureopacity",false));
	_ui->preview->setOpacityPressure(_ui->pressureopacity->isChecked());

	_ui->pressurehardness->setChecked(cfg.boolValue("pressurehardness",false));
	_ui->preview->setHardnessPressure(_ui->pressurehardness->isChecked());

	if(cfg.boolValue("hardedge", false))
		_ui->paintmodeHardedge->setChecked(true);
	else
		_ui->paintmodeSoftedge->setChecked(true);


	if(cfg.boolValue("incremental", true))
		_ui->paintmodeIncremental->setChecked(true);
	else
		_ui->paintmodeIndirect->setChecked(true);

	_ui->colorEraseMode->setChecked(cfg.boolValue("colorerase", false));

	_ui->preview->setIncremental(_ui->paintmodeIncremental->isChecked());
}

void EraserSettings::setForeground(const QColor& color)
{
	// Foreground color is used only in color-erase mode
	_ui->preview->setColor1(color);
}

void EraserSettings::setBackground(const QColor& color)
{
	// This is used just for the preview background color
	_ui->preview->setColor2(color);
}

void EraserSettings::quickAdjust1(float adjustment)
{
	int adj = qRound(adjustment);
	if(adj!=0)
		_ui->brushsize->setValue(_ui->brushsize->value() + adj);
}

paintcore::Brush EraserSettings::getBrush(bool swapcolors) const
{
	return _ui->preview->brush(swapcolors);
}

int EraserSettings::getSize() const
{
	return _ui->brushsize->value();
}

bool EraserSettings::getSubpixelMode() const
{
	return !_ui->paintmodeHardedge->isChecked();
}

BrushSettings::BrushSettings(QString name, QString title)
	: ToolSettings(name, title, "draw-brush"), _ui(0)
{
}

BrushSettings::~BrushSettings()
{
	delete _ui;
}

QWidget *BrushSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	_ui = new Ui_BrushSettings;
	_ui->setupUi(widget);
	populateBlendmodeBox(_ui->blendmode, _ui->preview);

	parent->connect(_ui->brushsize, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	parent->connect(_ui->preview, SIGNAL(requestFgColorChange()), parent, SLOT(changeForegroundColor()));
	parent->connect(_ui->preview, SIGNAL(requestBgColorChange()), parent, SLOT(changeBackgroundColor()));

	return widget;
}

ToolProperties BrushSettings::saveToolSettings()
{
	ToolProperties cfg;
	cfg.setValue("blendmode", _ui->blendmode->currentIndex());
	cfg.setValue("incremental", _ui->paintmodeIncremental->isChecked());
	cfg.setValue("size", _ui->brushsize->value());
	cfg.setValue("opacity", _ui->brushopacity->value());
	cfg.setValue("hardness", _ui->brushhardness->value());
	cfg.setValue("spacing", _ui->brushspacing->value());
	cfg.setValue("pressuresize", _ui->pressuresize->isChecked());
	cfg.setValue("pressureopacity", _ui->pressureopacity->isChecked());
	cfg.setValue("pressurehardness", _ui->pressurehardness->isChecked());
	return cfg;
}

void BrushSettings::restoreToolSettings(const ToolProperties &cfg)
{
	_ui->blendmode->setCurrentIndex(cfg.intValue("blendmode", 0));

	if(cfg.boolValue("incremental", true))
		_ui->paintmodeIncremental->setChecked(true);
	else
		_ui->paintmodeIndirect->setChecked(true);

	_ui->preview->setIncremental(_ui->paintmodeIncremental->isChecked());

	_ui->brushsize->setValue(cfg.intValue("size", 0));
	_ui->preview->setSize(_ui->brushsize->value());

	_ui->brushopacity->setValue(cfg.intValue("opacity", 100));
	_ui->preview->setOpacity(_ui->brushopacity->value());

	_ui->brushhardness->setValue(cfg.intValue("hardness", 50));
	_ui->preview->setHardness(_ui->brushhardness->value());

	_ui->brushspacing->setValue(cfg.intValue("spacing", 15));
	_ui->preview->setSpacing(_ui->brushspacing->value());

	_ui->pressuresize->setChecked(cfg.boolValue("pressuresize",false));
	_ui->preview->setSizePressure(_ui->pressuresize->isChecked());

	_ui->pressureopacity->setChecked(cfg.boolValue("pressureopacity",false));
	_ui->preview->setOpacityPressure(_ui->pressureopacity->isChecked());

	_ui->pressurehardness->setChecked(cfg.boolValue("pressurehardness",false));
	_ui->preview->setHardnessPressure(_ui->pressurehardness->isChecked());

	_ui->preview->setSubpixel(true);
}

void BrushSettings::setForeground(const QColor& color)
{
	_ui->preview->setColor1(color);
}

void BrushSettings::setBackground(const QColor& color)
{
	_ui->preview->setColor2(color);
}

void BrushSettings::quickAdjust1(float adjustment)
{
	int adj = qRound(adjustment);
	if(adj!=0)
		_ui->brushsize->setValue(_ui->brushsize->value() + adj);
}

paintcore::Brush BrushSettings::getBrush(bool swapcolors) const
{
	return _ui->preview->brush(swapcolors);
}

int BrushSettings::getSize() const
{
	return _ui->brushsize->value();
}

SmudgeSettings::SmudgeSettings(QString name, QString title)
	: ToolSettings(name, title, "draw-watercolor"), _ui(0)
{
}

SmudgeSettings::~SmudgeSettings()
{
	delete _ui;
}

QWidget *SmudgeSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	_ui = new Ui_SmudgeSettings;
	_ui->setupUi(widget);

	parent->connect(_ui->brushsize, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	parent->connect(_ui->preview, SIGNAL(requestFgColorChange()), parent, SLOT(changeForegroundColor()));
	parent->connect(_ui->preview, SIGNAL(requestBgColorChange()), parent, SLOT(changeBackgroundColor()));

	// Hardcoded value for now
	_ui->preview->setSmudgeFrequency(2);

	return widget;
}

ToolProperties SmudgeSettings::saveToolSettings()
{
	ToolProperties cfg;
	cfg.setValue("size", _ui->brushsize->value());
	cfg.setValue("opacity", _ui->brushopacity->value());
	cfg.setValue("hardness", _ui->brushhardness->value());
	cfg.setValue("spacing", _ui->brushspacing->value());
	cfg.setValue("smudge", _ui->brushsmudge->value());
	cfg.setValue("pressuresize", _ui->pressuresize->isChecked());
	cfg.setValue("pressureopacity", _ui->pressureopacity->isChecked());
	cfg.setValue("pressurehardness", _ui->pressurehardness->isChecked());
	cfg.setValue("pressuresmudge", _ui->pressuresmudge->isChecked());
	return cfg;
}

void SmudgeSettings::restoreToolSettings(const ToolProperties &cfg)
{
	_ui->brushsize->setValue(cfg.intValue("size", 0));
	_ui->preview->setSize(_ui->brushsize->value());

	_ui->brushopacity->setValue(cfg.intValue("opacity", 100));
	_ui->preview->setOpacity(_ui->brushopacity->value());

	_ui->brushhardness->setValue(cfg.intValue("hardness", 50));
	_ui->preview->setHardness(_ui->brushhardness->value());

	_ui->brushsmudge->setValue(cfg.intValue("smudge", 50));
	_ui->preview->setSmudge(_ui->brushsmudge->value());

	_ui->brushspacing->setValue(cfg.intValue("spacing", 15));
	_ui->preview->setSpacing(_ui->brushspacing->value());

	_ui->pressuresize->setChecked(cfg.boolValue("pressuresize",false));
	_ui->preview->setSizePressure(_ui->pressuresize->isChecked());

	_ui->pressureopacity->setChecked(cfg.boolValue("pressureopacity",false));
	_ui->preview->setOpacityPressure(_ui->pressureopacity->isChecked());

	_ui->pressurehardness->setChecked(cfg.boolValue("pressurehardness",false));
	_ui->preview->setHardnessPressure(_ui->pressurehardness->isChecked());

	_ui->pressuresmudge->setChecked(cfg.boolValue("pressuresmudge",false));
	_ui->preview->setSmudgePressure(_ui->pressuresmudge->isChecked());

	_ui->preview->setSubpixel(true);
}

void SmudgeSettings::setForeground(const QColor& color)
{
	_ui->preview->setColor1(color);
}

void SmudgeSettings::setBackground(const QColor& color)
{
	_ui->preview->setColor2(color);
}

void SmudgeSettings::quickAdjust1(float adjustment)
{
	int adj = qRound(adjustment);
	if(adj!=0)
		_ui->brushsize->setValue(_ui->brushsize->value() + adj);
}

paintcore::Brush SmudgeSettings::getBrush(bool swapcolors) const
{
	return _ui->preview->brush(swapcolors);
}

int SmudgeSettings::getSize() const
{
	return _ui->brushsize->value();
}


void BrushlessSettings::setForeground(const QColor& color)
{
	_dummybrush.setColor(color);
}

void BrushlessSettings::setBackground(const QColor& color)
{
	_dummybrush.setColor2(color);
}

paintcore::Brush BrushlessSettings::getBrush(bool swapcolors) const
{
	Q_UNUSED(swapcolors);
	return _dummybrush;
}

LaserPointerSettings::LaserPointerSettings(const QString &name, const QString &title)
	: QObject(), BrushlessSettings(name, title, "cursor-arrow"), _ui(0)
{
}

LaserPointerSettings::~LaserPointerSettings()
{
	delete _ui;
}

QWidget *LaserPointerSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	_ui = new Ui_LaserSettings;
	_ui->setupUi(widget);

	connect(_ui->trackpointer, SIGNAL(clicked(bool)), this, SIGNAL(pointerTrackingToggled(bool)));

	return widget;
}

ToolProperties LaserPointerSettings::saveToolSettings()
{
	ToolProperties cfg;
	cfg.setValue("tracking", _ui->trackpointer->isChecked());
	cfg.setValue("persistence", _ui->persistence->value());

	int color=0;

	if(_ui->color1->isChecked())
		color=1;
	else if(_ui->color2->isChecked())
		color=2;
	else if(_ui->color3->isChecked())
		color=3;
	cfg.setValue("color", color);

	return cfg;
}

void LaserPointerSettings::restoreToolSettings(const ToolProperties &cfg)
{
	_ui->trackpointer->setChecked(cfg.boolValue("tracking", true));
	_ui->persistence->setValue(cfg.intValue("persistence", 1));

	switch(cfg.intValue("color", 0, 0, 3)) {
	case 0: _ui->color0->setChecked(true); break;
	case 1: _ui->color1->setChecked(true); break;
	case 2: _ui->color2->setChecked(true); break;
	case 3: _ui->color3->setChecked(true); break;
	}
}

bool LaserPointerSettings::pointerTracking() const
{
	return _ui->trackpointer->isChecked();
}

int LaserPointerSettings::trailPersistence() const
{
	return _ui->persistence->value();
}

void LaserPointerSettings::setForeground(const QColor &color)
{
	_ui->color0->setColor(color);
}

void LaserPointerSettings::quickAdjust1(float adjustment)
{
	int adj = qRound(adjustment);
	if(adj!=0)
		_ui->persistence->setValue(_ui->persistence->value() + adj);
}

paintcore::Brush LaserPointerSettings::getBrush(bool swapcolors) const
{
	QColor c;
	if(swapcolors)
		c = _dummybrush.color2();
	else {
		if(_ui->color0->isChecked())
			c = _ui->color0->color();
		else if(_ui->color1->isChecked())
			c = _ui->color1->color();
		else if(_ui->color2->isChecked())
			c = _ui->color2->color();
		else if(_ui->color3->isChecked())
			c = _ui->color3->color();
	}
	const_cast<paintcore::Brush&>(_dummybrush).setColor(c);
	return _dummybrush;
}

SimpleSettings::SimpleSettings(const QString &name, const QString &title, const QString &icon, Type type, bool sp)
	: ToolSettings(name, title, icon), _ui(0), _type(type), _subpixel(sp)
{
}

SimpleSettings::~SimpleSettings()
{
	delete _ui;
}

QWidget *SimpleSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	_ui = new Ui_SimpleSettings;
	_ui->setupUi(widget);

	populateBlendmodeBox(_ui->blendmode, _ui->preview);

	// Connect size change signal
	parent->connect(_ui->brushsize, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	parent->connect(_ui->paintmodeHardedge, &QToolButton::toggled, [this](bool hard) {
		_ui->brushhardness->setEnabled(!hard);
		_ui->spinHardness->setEnabled(!hard);
	});

	// Set proper preview shape
	if(_type==Line)
		_ui->preview->setPreviewShape(BrushPreview::Line);
	else if(_type==Rectangle)
		_ui->preview->setPreviewShape(BrushPreview::Rectangle);
	else if(_type==Ellipse)
		_ui->preview->setPreviewShape(BrushPreview::Ellipse);

	_ui->preview->setSubpixel(_subpixel);

	if(!_subpixel) {
		// Hard edge mode is always enabled for tools that do not support antialiasing
		_ui->paintmodeHardedge->hide();
		_ui->paintmodeSmoothedge->hide();
	} else {
		parent->connect(_ui->paintmodeHardedge, SIGNAL(toggled(bool)), parent, SLOT(updateSubpixelMode()));
	}

	parent->connect(_ui->preview, SIGNAL(requestFgColorChange()), parent, SLOT(changeForegroundColor()));
	parent->connect(_ui->preview, SIGNAL(requestBgColorChange()), parent, SLOT(changeBackgroundColor()));

	return widget;
}

ToolProperties SimpleSettings::saveToolSettings()
{
	ToolProperties cfg;
	cfg.setValue("blendmode", _ui->blendmode->currentIndex());
	cfg.setValue("size", _ui->brushsize->value());
	cfg.setValue("opacity", _ui->brushopacity->value());
	cfg.setValue("hardness", _ui->brushhardness->value());
	cfg.setValue("spacing", _ui->brushspacing->value());
	cfg.setValue("hardedge", _ui->paintmodeHardedge->isChecked());
	cfg.setValue("incremental", _ui->paintmodeIncremental->isChecked());
	return cfg;
}

void SimpleSettings::restoreToolSettings(const ToolProperties &cfg)
{
	_ui->blendmode->setCurrentIndex(cfg.intValue("blendmode", 0));

	_ui->brushsize->setValue(cfg.intValue("size", 0));
	_ui->preview->setSize(_ui->brushsize->value());

	_ui->brushopacity->setValue(cfg.intValue("opacity", 100));
	_ui->preview->setOpacity(_ui->brushopacity->value());

	_ui->brushhardness->setValue(cfg.intValue("hardness", 50));
	_ui->preview->setHardness(_ui->brushhardness->value());

	_ui->brushspacing->setValue(cfg.intValue("spacing", 15));
	_ui->preview->setSpacing(_ui->brushspacing->value());

	if(cfg.boolValue("hardedge", false))
		_ui->paintmodeHardedge->setChecked(true);
	else
		_ui->paintmodeSmoothedge->setChecked(true);

	if(cfg.boolValue("incremental", true))
		_ui->paintmodeIncremental->setChecked(true);
	else
		_ui->paintmodeIndirect->setChecked(true);

	_ui->preview->setIncremental(_ui->paintmodeIncremental->isChecked());
}

void SimpleSettings::setForeground(const QColor& color)
{
	_ui->preview->setColor1(color);
}

void SimpleSettings::setBackground(const QColor& color)
{
	_ui->preview->setColor2(color);
}

void SimpleSettings::quickAdjust1(float adjustment)
{
	int adj = qRound(adjustment);
	if(adj!=0)
		_ui->brushsize->setValue(_ui->brushsize->value() + adj);
}

paintcore::Brush SimpleSettings::getBrush(bool swapcolors) const
{
	return _ui->preview->brush(swapcolors);
}

int SimpleSettings::getSize() const
{
	return _ui->brushsize->value();
}

bool SimpleSettings::getSubpixelMode() const
{
	return !_ui->paintmodeHardedge->isChecked();
}

ColorPickerSettings::ColorPickerSettings(const QString &name, const QString &title)
	:  QObject(), BrushlessSettings(name, title, "color-picker"), _layerpick(0)
{
	_palette.setColumns(8);
}

ColorPickerSettings::~ColorPickerSettings()
{
}

QWidget *ColorPickerSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	QVBoxLayout *layout = new QVBoxLayout(widget);
	layout->setMargin(3);
	widget->setLayout(layout);

	QHBoxLayout *sizelayout = new QHBoxLayout;
	layout->addLayout(sizelayout);

	QLabel *sizelbl = new QLabel(tr("Size:"), widget);
	sizelayout->addWidget(sizelbl);

	QSlider *slider = new QSlider(widget);
	slider->setOrientation(Qt::Horizontal);
	sizelayout->addWidget(slider);

	m_size = new QSpinBox(widget);
	sizelayout->addWidget(m_size);

	m_size->setMinimum(1);
	slider->setMinimum(1);

	m_size->setMaximum(128);
	slider->setMaximum(128);

	connect(m_size, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	connect(slider, &QSlider::valueChanged, m_size, &QSpinBox::setValue);
	connect(m_size, SIGNAL(valueChanged(int)), slider, SLOT(setValue(int)));

	_layerpick = new QCheckBox(tr("Pick from current layer only"), widget);
	layout->addWidget(_layerpick);

	_palettewidget = new widgets::PaletteWidget(widget);
	_palettewidget->setPalette(&_palette);
	layout->addWidget(_palettewidget);

	connect(_palettewidget, SIGNAL(colorSelected(QColor)), this, SIGNAL(colorSelected(QColor)));

	return widget;
}

int ColorPickerSettings::getSize() const
{
	return m_size->value();
}

void ColorPickerSettings::quickAdjust1(float adjustment)
{
	int adj = qRound(adjustment);
	if(adj!=0)
		m_size->setValue(m_size->value() + adj);
}

ToolProperties ColorPickerSettings::saveToolSettings()
{
	ToolProperties cfg;
	cfg.setValue("layerpick", _layerpick->isChecked());
	cfg.setValue("size", m_size->value());
	return cfg;
}

void ColorPickerSettings::restoreToolSettings(const ToolProperties &cfg)
{
	_layerpick->setChecked(cfg.boolValue("layerpick", false));
	m_size->setValue(cfg.intValue("size", 1));
}

bool ColorPickerSettings::pickFromLayer() const
{
	return _layerpick->isChecked();
}

void ColorPickerSettings::addColor(const QColor &color)
{
	if(_palette.count() && _palette.color(0).color == color)
		return;

	_palette.insertColor(0, color);

	if(_palette.count() > 80)
		_palette.removeColor(_palette.count()-1);

	_palettewidget->update();
}

AnnotationSettings::AnnotationSettings(QString name, QString title)
	: QObject(), BrushlessSettings(name, title, "draw-text"), _ui(0), _noupdate(false), m_mergeEnabled(true)
{
}

AnnotationSettings::~AnnotationSettings()
{
	delete _ui;
}

QWidget *AnnotationSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	_ui = new Ui_TextSettings;
	_ui->setupUi(widget);

	_updatetimer = new QTimer(this);
	_updatetimer->setInterval(500);
	_updatetimer->setSingleShot(true);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
	// Set editor placeholder
	_ui->content->setPlaceholderText(tr("Annotation content"));
#endif

	// Editor events
	connect(_ui->content, SIGNAL(textChanged()), this, SLOT(applyChanges()));
	connect(_ui->content, SIGNAL(cursorPositionChanged()), this, SLOT(updateStyleButtons()));

	connect(_ui->btnBackground, SIGNAL(colorChanged(const QColor&)),
			this, SLOT(setEditorBackgroundColor(const QColor &)));
	connect(_ui->btnBackground, SIGNAL(colorChanged(const QColor&)),
			this, SLOT(applyChanges()));

	connect(_ui->btnRemove, SIGNAL(clicked()), this, SLOT(removeAnnotation()));
	connect(_ui->btnBake, SIGNAL(clicked()), this, SLOT(bake()));

	connect(_ui->font, SIGNAL(currentIndexChanged(int)), this, SLOT(updateFontIfUniform()));
	connect(_ui->size, SIGNAL(valueChanged(double)), this, SLOT(updateFontIfUniform()));
	connect(_ui->btnTextColor, SIGNAL(colorChanged(QColor)), this, SLOT(updateFontIfUniform()));

	// Intra-editor connections that couldn't be made in the UI designer
	connect(_ui->left, SIGNAL(clicked()), this, SLOT(changeAlignment()));
	connect(_ui->center, SIGNAL(clicked()), this, SLOT(changeAlignment()));
	connect(_ui->justify, SIGNAL(clicked()), this, SLOT(changeAlignment()));
	connect(_ui->right, SIGNAL(clicked()), this, SLOT(changeAlignment()));
	connect(_ui->bold, SIGNAL(toggled(bool)), this, SLOT(toggleBold(bool)));
	connect(_ui->strikethrough, SIGNAL(toggled(bool)), this, SLOT(toggleStrikethrough(bool)));

	connect(_updatetimer, SIGNAL(timeout()), this, SLOT(saveChanges()));

	// Select a nice default font
	QStringList defaultFonts;
	defaultFonts << "Arial" << "Helvetica" << "Sans Serif";
	for(const QString &df : defaultFonts) {
		int i = _ui->font->findText(df, Qt::MatchFixedString);
		if(i>=0) {
			_ui->font->setCurrentIndex(i);
			break;
		}
	}

	setUiEnabled(false);

	return widget;
}

void AnnotationSettings::setUiEnabled(bool enabled)
{
	_ui->content->setEnabled(enabled);
	_ui->btnBake->setEnabled(enabled & m_mergeEnabled);
	_ui->btnRemove->setEnabled(enabled);
}

void AnnotationSettings::setEditorBackgroundColor(const QColor &color)
{
	// Blend transparent colors with white
	const QColor c = QColor::fromRgbF(
		color.redF() * color.alphaF() + (1-color.alphaF()),
		color.greenF() * color.alphaF() + (1-color.alphaF()),
		color.blueF() * color.alphaF() + (1-color.alphaF())
	);

	// We need to use the stylesheet because native styles ignore the palette.
	_ui->content->setStyleSheet("background-color: " + c.name());
}

void AnnotationSettings::updateStyleButtons()
{
	QTextBlockFormat bf = _ui->content->textCursor().blockFormat();
	switch(bf.alignment()) {
	case Qt::AlignLeft: _ui->left->setChecked(true); break;
	case Qt::AlignCenter: _ui->center->setChecked(true); break;
	case Qt::AlignJustify: _ui->justify->setChecked(true); break;
	case Qt::AlignRight: _ui->right->setChecked(true); break;
	default: break;
	}
	QTextCharFormat cf = _ui->content->textCursor().charFormat();
	_ui->btnTextColor->setColor(cf.foreground().color());

	_ui->size->blockSignals(true);
	if(cf.fontPointSize() < 1)
		_ui->size->setValue(12);
	else
		_ui->size->setValue(cf.fontPointSize());
	_ui->size->blockSignals(false);

	_ui->font->blockSignals(true);
	_ui->font->setCurrentFont(cf.font());
	_ui->font->blockSignals(false);

	_ui->italic->setChecked(cf.fontItalic());
	_ui->bold->setChecked(cf.fontWeight() > QFont::Normal);
	_ui->underline->setChecked(cf.fontUnderline());
	_ui->strikethrough->setChecked(cf.font().strikeOut());
}

void AnnotationSettings::toggleBold(bool bold)
{
	_ui->content->setFontWeight(bold ? QFont::Bold : QFont::Normal);
}

void AnnotationSettings::toggleStrikethrough(bool strike)
{
	QFont font = _ui->content->currentFont();
	font.setStrikeOut(strike);
	_ui->content->setCurrentFont(font);
}

void AnnotationSettings::changeAlignment()
{
	Qt::Alignment a = Qt::AlignLeft;
	if(_ui->left->isChecked())
		a = Qt::AlignLeft;
	else if(_ui->center->isChecked())
		a = Qt::AlignCenter;
	else if(_ui->justify->isChecked())
		a = Qt::AlignJustify;
	else if(_ui->right->isChecked())
		a = Qt::AlignRight;

	_ui->content->setAlignment(a);
}

void AnnotationSettings::updateFontIfUniform()
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 3, 0))
	bool uniformFontFamily = true;
	bool uniformSize = true;
	bool uniformColor = true;

	QTextDocument *doc = _ui->content->document();

	QTextBlock b = doc->firstBlock();
	QTextCharFormat fmt1;
	bool first=true;

	// Check all character formats in all blocks. If they are the same,
	// we can reset the font for the wole document.
	while(b.isValid()) {
		for(const QTextLayout::FormatRange &fr : b.textFormats()) {

			if(first) {
				fmt1 = fr.format;
				first = false;

			} else {
				uniformFontFamily &= fr.format.fontFamily() == fmt1.fontFamily();
				uniformSize &= fr.format.fontPointSize() == fmt1.fontPointSize();
				uniformColor &= fr.format.foreground() == fmt1.foreground();
			}
		}
		b = b.next();
	}

	resetContentFont(uniformFontFamily, uniformSize, uniformColor);
#endif
}

void AnnotationSettings::resetContentFont(bool resetFamily, bool resetSize, bool resetColor)
{
	if(!(resetFamily|resetSize|resetColor))
		return;

	QTextCursor cursor(_ui->content->document());
	cursor.select(QTextCursor::Document);
	QTextCharFormat fmt;

	if(resetFamily)
		fmt.setFontFamily(_ui->font->currentText());

	if(resetSize)
		fmt.setFontPointSize(_ui->size->value());

	if(resetColor)
		fmt.setForeground(_ui->btnTextColor->color());

	cursor.mergeCharFormat(fmt);
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

void AnnotationSettings::setMergeEnabled(bool enable)
{
	m_mergeEnabled = enable;
	_ui->btnBake->setEnabled(enable && selected());
}

void AnnotationSettings::setSelection(drawingboard::AnnotationItem *item)
{
	_noupdate = true;
	setUiEnabled(item!=0);

	if(_selection)
		_selection->setHighlight(false);

	_selection = item;
	if(item) {
		item->setHighlight(true);
		const paintcore::Annotation *a = item->getAnnotation();
		Q_ASSERT(a);
		_ui->content->setHtml(a->text());
		_ui->btnBackground->setColor(a->backgroundColor());
		_ui->ownerLabel->setText(QString("(%1)").arg(_client->userlist()->getUsername((a->id() & 0xff00) >> 8)));
		setEditorBackgroundColor(a->backgroundColor());
		if(a->text().isEmpty())
			resetContentFont(true, true, true);

	}
	_noupdate = false;
}

void AnnotationSettings::setFocusAt(int cursorPos)
{
	_ui->content->setFocus();
	if(cursorPos>=0) {
		QTextCursor c = _ui->content->textCursor();
		c.setPosition(cursorPos);
		_ui->content->setTextCursor(c);
	}
}

void AnnotationSettings::setFocus()
{
	setFocusAt(-1);
}

void AnnotationSettings::applyChanges()
{
	if(_noupdate || !selected())
		return;
	_updatetimer->start();
}

void AnnotationSettings::saveChanges()
{
	if(!selected())
		return;

	Q_ASSERT(_client);

	if(selected())
		_client->sendAnnotationEdit(
			selected(),
			_ui->btnBackground->color(),
			_ui->content->toHtml()
		);
}

void AnnotationSettings::removeAnnotation()
{
	Q_ASSERT(selected());
	Q_ASSERT(_client);
	_client->sendUndopoint();
	_client->sendAnnotationDelete(selected());
	setSelection(0); /* not strictly necessary, but makes the UI seem more responsive */
}

void AnnotationSettings::bake()
{
	Q_ASSERT(selected());
	Q_ASSERT(_layerlist);
	Q_ASSERT(_client);

	const paintcore::Annotation *a = _selection->getAnnotation();
	Q_ASSERT(a);

	QImage img = a->toImage();

	int layer = _layerlist->currentLayer();
	_client->sendUndopoint();
	_client->sendImage(layer, a->rect().x(), a->rect().y(), img);
	_client->sendAnnotationDelete(selected());
	setSelection(0); /* not strictly necessary, but makes the UI seem more responsive */
}

SelectionSettings::SelectionSettings(const QString &name, const QString &title, bool freeform)
	: QObject(), BrushlessSettings(name, title, freeform ? "edit-select-lasso" : "select-rectangular"), _ui(0)
{
}

SelectionSettings::~SelectionSettings()
{
	delete _ui;
}

QWidget *SelectionSettings::createUiWidget(QWidget *parent)
{
	QWidget *uiwidget = new QWidget(parent);
	_ui = new Ui_SelectionSettings;
	_ui->setupUi(uiwidget);

	connect(_ui->flip, SIGNAL(clicked()), this, SLOT(flipSelection()));
	connect(_ui->mirror, SIGNAL(clicked()), this, SLOT(mirrorSelection()));
	connect(_ui->fittoscreen, SIGNAL(clicked()), this, SLOT(fitToScreen()));
	connect(_ui->resetsize, SIGNAL(clicked()), this, SLOT(resetSize()));

	return uiwidget;
}

void SelectionSettings::flipSelection()
{
	drawingboard::SelectionItem *sel = _scene->selectionItem();
	if(sel) {
		cutSelection();
		sel->scale(1, -1);
	}

}

void SelectionSettings::mirrorSelection()
{
	drawingboard::SelectionItem *sel = _scene->selectionItem();
	if(sel) {
		cutSelection();
		sel->scale(-1, 1);
	}
}

void SelectionSettings::fitToScreen()
{
	drawingboard::SelectionItem *sel = _scene->selectionItem();
	if(sel) {
		cutSelection();
		const QSizeF size = sel->polygonRect().size();
		const QRectF screenRect = _view->mapToScene(_view->rect()).boundingRect();
		const QSizeF screen = screenRect.size() * 0.7;

		if(size.width() > screen.width() || size.height() > screen.height()) {
			const QSizeF newsize = size.scaled(screen, Qt::KeepAspectRatio);
			sel->scale(newsize.width() / size.width(), newsize.height() / size.height());
		}

		if(!sel->polygonRect().intersects(screenRect.toRect())) {
			QPointF offset = screenRect.center() - sel->polygonRect().center();
			sel->translate(offset.toPoint());
		}
	}
}

void SelectionSettings::resetSize()
{
	drawingboard::SelectionItem *sel = _scene->selectionItem();
	if(sel)
		sel->resetPolygonShape();
}

void SelectionSettings::cutSelection()
{
	Q_ASSERT(_scene->selectionItem());
	const int layer = _layerlist->currentLayer();
	if(_scene->selectionItem()->pasteImage().isNull() && !_scene->statetracker()->isLayerLocked(layer)) {
		// Automatically cut the layer when the selection is transformed
		QImage img = _scene->selectionToImage(layer);
		_scene->selectionItem()->fillCanvas(Qt::white, paintcore::BlendMode::MODE_ERASE, _client, layer);
		_scene->selectionItem()->setPasteImage(img);
		_scene->selectionItem()->setMovedFromCanvas(true);
	}
}

FillSettings::FillSettings(const QString &name, const QString &title)
	: BrushlessSettings(name, title, "fill-color"), _ui(0)
{
}

FillSettings::~FillSettings()
{
	delete _ui;
}

QWidget *FillSettings::createUiWidget(QWidget *parent)
{
	QWidget *uiwidget = new QWidget(parent);
	_ui = new Ui_FillSettings;
	_ui->setupUi(uiwidget);

	parent->connect(_ui->preview, SIGNAL(requestFgColorChange()), parent, SLOT(changeForegroundColor()));
	parent->connect(_ui->preview, SIGNAL(requestBgColorChange()), parent, SLOT(changeBackgroundColor()));

	return uiwidget;
}

int FillSettings::fillTolerance() const
{
	return _ui->tolerance->value();
}

int FillSettings::fillExpansion() const
{
	return _ui->expand->value();
}

bool FillSettings::sampleMerged() const
{
	return _ui->samplemerged->isChecked();
}

bool FillSettings::underFill() const
{
	return _ui->fillunder->isChecked();
}

ToolProperties FillSettings::saveToolSettings()
{
	ToolProperties cfg;
	cfg.setValue("tolerance", fillTolerance());
	cfg.setValue("expand", fillExpansion());
	cfg.setValue("samplemerged", sampleMerged());
	cfg.setValue("underfill", underFill());
	return cfg;
}

void FillSettings::setForeground(const QColor &color)
{
	// note: colors are swapped, because brushpreview widget uses
	// the background color as the fill color
	_ui->preview->setColor2(color);
	BrushlessSettings::setForeground(color);
}

void FillSettings::setBackground(const QColor &color)
{
	_ui->preview->setColor1(color);
	BrushlessSettings::setBackground(color);
}

void FillSettings::restoreToolSettings(const ToolProperties &cfg)
{
	_ui->tolerance->setValue(cfg.value("tolerance", 0).toInt());
	_ui->expand->setValue(cfg.value("expand", 0).toInt());
	_ui->samplemerged->setChecked(cfg.value("samplemerged", true).toBool());
	_ui->fillunder->setChecked(cfg.value("underfill", true).toBool());
}

void FillSettings::quickAdjust1(float adjustment)
{
	int adj = qRound(adjustment);
	if(adj!=0)
		_ui->tolerance->setValue(_ui->tolerance->value() + adj);
}


}
