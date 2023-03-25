/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2021 Calle Laakkonen

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

#include "desktop/toolwidgets/fillsettings.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/toolproperties.h"
#include "libclient/tools/floodfill.h"

#include "ui_fillsettings.h"

static constexpr int SOURCE_MERGED_IMAGE = 1;

namespace tools {

namespace props {
	static const ToolProperties::RangedValue<int>
		expand { QStringLiteral("expand"), 0, 0, 100 },
		featherRadius { QStringLiteral("featherRadius"), 0, 0, 40 },
		source { QStringLiteral("source"), 0, 0, 1},
		mode { QStringLiteral("mode"), 0, 0, 2};
	static const ToolProperties::RangedValue<double>
		tolerance { QStringLiteral("tolerance"), 0.0, 0.0, 1.0 },
		sizelimit { QStringLiteral("sizelimit"), 50.0, 0.0, 1000.0 };
}

FillSettings::FillSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent), m_ui(nullptr)
{
}

FillSettings::~FillSettings()
{
	delete m_ui;
}

QWidget *FillSettings::createUiWidget(QWidget *parent)
{
	QWidget *uiwidget = new QWidget(parent);
	m_ui = new Ui_FillSettings;
	m_ui->setupUi(uiwidget);

	m_ui->preview->setPreviewShape(DP_BRUSH_PREVIEW_FLOOD_FILL);

	connect(m_ui->preview, SIGNAL(requestColorChange()), parent, SLOT(changeForegroundColor()));
	connect(m_ui->tolerance, QOverload<int>::of(&QSpinBox::valueChanged), this, &FillSettings::pushSettings);
	connect(m_ui->sizelimit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FillSettings::pushSettings);
	connect(m_ui->expand, QOverload<int>::of(&QSpinBox::valueChanged), this, &FillSettings::pushSettings);
	connect(m_ui->feather, QOverload<int>::of(&QSpinBox::valueChanged), this, &FillSettings::pushSettings);
	connect(m_ui->source, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FillSettings::pushSettings);
	connect(m_ui->mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FillSettings::pushSettings);
	return uiwidget;
}

void FillSettings::pushSettings()
{
	FloodFill *tool = static_cast<FloodFill*>(controller()->getTool(Tool::FLOODFILL));
	tool->setTolerance(m_ui->tolerance->value() / qreal(m_ui->tolerance->maximum()));
	tool->setExpansion(m_ui->expand->value());
	tool->setFeatherRadius(m_ui->feather->value());
	tool->setSizeLimit(m_ui->sizelimit->value() * m_ui->sizelimit->value() * 10 * 10);
	tool->setSampleMerged(m_ui->source->currentIndex() == SOURCE_MERGED_IMAGE);
	const auto mode = static_cast<Mode>(m_ui->mode->currentIndex());
	tool->setBlendMode(modeIndexToBlendMode(mode));
	if(mode == Erase) {
		m_ui->preview->setPreviewShape(DP_BRUSH_PREVIEW_FLOOD_ERASE);
	} else {
		m_ui->preview->setPreviewShape(DP_BRUSH_PREVIEW_FLOOD_FILL);
		m_previousMode = mode;
	}
}

void FillSettings::toggleEraserMode()
{
	int mode = m_ui->mode->currentIndex();
	m_ui->mode->setCurrentIndex(mode == Erase ? m_previousMode : Erase);
}

ToolProperties FillSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue(props::tolerance, m_ui->tolerance->value() / qreal(m_ui->tolerance->maximum()));
	cfg.setValue(props::expand, m_ui->expand->value());
	cfg.setValue(props::featherRadius, m_ui->feather->value());
	cfg.setValue(props::mode, m_ui->mode->currentIndex());
	cfg.setValue(props::source, m_ui->source->currentIndex());
	return cfg;
}

void FillSettings::setForeground(const QColor &color)
{
	brushes::ActiveBrush b;
	b.classic().size.max = 1;
	b.setQColor(color);
	m_ui->preview->setBrush(b);
	controller()->setActiveBrush(b);
}

void FillSettings::restoreToolSettings(const ToolProperties &cfg)
{
	m_ui->tolerance->setValue(cfg.value(props::tolerance) * m_ui->tolerance->maximum());
	m_ui->expand->setValue(cfg.value(props::expand));
	m_ui->feather->setValue(cfg.value(props::featherRadius));
	m_ui->sizelimit->setValue(cfg.value(props::sizelimit));
	m_ui->source->setCurrentIndex(cfg.value(props::source));
	m_ui->mode->setCurrentIndex(cfg.value(props::mode));
	pushSettings();
}

int FillSettings::modeIndexToBlendMode(int mode)
{
	switch(mode) {
	case Behind:
		return DP_BLEND_MODE_BEHIND;
	case Erase:
		return DP_BLEND_MODE_ERASE;
	default:
		return DP_BLEND_MODE_NORMAL;
	}
}

void FillSettings::quickAdjust1(qreal adjustment)
{
	m_quickAdjust1 += adjustment;
	qreal i;
	qreal f = modf(m_quickAdjust1, &i);
	if(int(i)) {
		m_quickAdjust1 = f;
		m_ui->tolerance->setValue(m_ui->tolerance->value() + int(i));
	}
}

void FillSettings::stepAdjust1(bool increase)
{
	QSpinBox *tolerance = m_ui->tolerance;
	tolerance->setValue(stepLogarithmic(
		tolerance->minimum(), tolerance->maximum(), tolerance->value(),
		increase));
}

}

