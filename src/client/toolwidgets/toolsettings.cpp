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
#include "tools/toolproperties.h"
#include "tools/toolcontroller.h"
#include "widgets/brushpreview.h"

#include "core/blendmodes.h"

#include <QComboBox>

namespace tools {

void populateBlendmodeBox(QComboBox *box, widgets::BrushPreview *preview) {
	for(const auto bm : paintcore::getBlendModeNames(paintcore::BlendMode::BrushMode))
		box->addItem(bm.second, bm.first);

	preview->setBlendingMode(paintcore::BlendMode::MODE_NORMAL);
	box->connect(box, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [box,preview](int) {
		preview->setBlendingMode(paintcore::BlendMode::Mode(box->currentData().toInt()));
	});
}

QWidget *ToolSettings::createUi(QWidget *parent)
{
	Q_ASSERT(m_widget==0);
	m_widget = createUiWidget(parent);
	return m_widget;
}

ToolProperties ToolSettings::saveToolSettings()
{
	return ToolProperties();
}

void ToolSettings::restoreToolSettings(const ToolProperties &)
{
}


void BrushlessSettings::setForeground(const QColor& color)
{
	paintcore::Brush b;
	b.setColor(color);
	controller()->setActiveBrush(b);
}

}

