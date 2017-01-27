/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2017 Calle Laakkonen

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

#include "shapetoolsettings.h"
#include "tools/toolcontroller.h"
#include "tools/toolproperties.h"

// Work around lack of namespace support in Qt designer (TODO is the problem in our plugin?)
#include "widgets/groupedtoolbutton.h"
#include "widgets/brushpreview.h"
using widgets::BrushPreview;
using widgets::GroupedToolButton;

#include "ui_simplesettings.h"

namespace tools {

SimpleSettings::SimpleSettings(const QString &name, const QString &title, const QString &icon, Type type, bool sp, ToolController *ctrl)
	: ToolSettings(name, title, icon, ctrl), m_ui(nullptr), m_type(type), m_subpixel(sp)
{
}

SimpleSettings::~SimpleSettings()
{
	delete m_ui;
}

tools::Tool::Type SimpleSettings::toolType() const {
	switch(m_type) {
	case SimpleSettings::Line: return tools::Tool::LINE;
	case SimpleSettings::Rectangle: return tools::Tool::RECTANGLE;
	case SimpleSettings::Ellipse: break;
	}
	return tools::Tool::ELLIPSE;
}


QWidget *SimpleSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	m_ui = new Ui_SimpleSettings;
	m_ui->setupUi(widget);

	populateBlendmodeBox(m_ui->blendmode, m_ui->preview);

	// Connect size change signal
	parent->connect(m_ui->brushsize, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	parent->connect(m_ui->paintmodeHardedge, &QToolButton::toggled, [this](bool hard) {
		m_ui->brushhardness->setEnabled(!hard);
		m_ui->spinHardness->setEnabled(!hard);
	});

	// Set proper preview shape
	switch(m_type) {
	case Line: m_ui->preview->setPreviewShape(BrushPreview::Line); break;
	case Rectangle: m_ui->preview->setPreviewShape(BrushPreview::Rectangle); break;
	case Ellipse: m_ui->preview->setPreviewShape(BrushPreview::Ellipse); break;
	}

	m_ui->preview->setSubpixel(m_subpixel);

	if(!m_subpixel) {
		// Hard edge mode is always enabled for tools that do not support antialiasing
		m_ui->paintmodeHardedge->hide();
		m_ui->paintmodeSmoothedge->hide();
	} else {
		parent->connect(m_ui->paintmodeHardedge, SIGNAL(toggled(bool)), parent, SLOT(updateSubpixelMode()));
	}

	parent->connect(m_ui->preview, SIGNAL(requestColorChange()), parent, SLOT(changeForegroundColor()));
	parent->connect(m_ui->preview, &BrushPreview::brushChanged, controller(), &ToolController::setActiveBrush);

	return widget;
}

ToolProperties SimpleSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue("blendmode", m_ui->blendmode->currentIndex());
	cfg.setValue("size", m_ui->brushsize->value());
	cfg.setValue("opacity", m_ui->brushopacity->value());
	cfg.setValue("hardness", m_ui->brushhardness->value());
	cfg.setValue("spacing", m_ui->brushspacing->value());
	cfg.setValue("hardedge", m_ui->paintmodeHardedge->isChecked());
	cfg.setValue("incremental", m_ui->paintmodeIncremental->isChecked());
	return cfg;
}

void SimpleSettings::restoreToolSettings(const ToolProperties &cfg)
{
	m_ui->blendmode->setCurrentIndex(cfg.intValue("blendmode", 0));

	m_ui->brushsize->setValue(cfg.intValue("size", 0));
	m_ui->preview->setSize(m_ui->brushsize->value());

	m_ui->brushopacity->setValue(cfg.intValue("opacity", 100));
	m_ui->preview->setOpacity(m_ui->brushopacity->value());

	m_ui->brushhardness->setValue(cfg.intValue("hardness", 50));
	m_ui->preview->setHardness(m_ui->brushhardness->value());

	m_ui->brushspacing->setValue(cfg.intValue("spacing", 15));
	m_ui->preview->setSpacing(m_ui->brushspacing->value());

	if(cfg.boolValue("hardedge", false))
		m_ui->paintmodeHardedge->setChecked(true);
	else
		m_ui->paintmodeSmoothedge->setChecked(true);

	if(cfg.boolValue("incremental", true))
		m_ui->paintmodeIncremental->setChecked(true);
	else
		m_ui->paintmodeIndirect->setChecked(true);

	m_ui->preview->setIncremental(m_ui->paintmodeIncremental->isChecked());
}

void SimpleSettings::setForeground(const QColor& color)
{
	m_ui->preview->setColor(color);
}

void SimpleSettings::quickAdjust1(float adjustment)
{
	int adj = qRound(adjustment);
	if(adj!=0)
		m_ui->brushsize->setValue(m_ui->brushsize->value() + adj);
}

int SimpleSettings::getSize() const
{
	return m_ui->brushsize->value();
}

bool SimpleSettings::getSubpixelMode() const
{
	return !m_ui->paintmodeHardedge->isChecked();
}

}

