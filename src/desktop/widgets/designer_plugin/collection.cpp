/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

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

#include "desktop/widgets/designer_plugin/collection.h"
#include "desktop/widgets/designer_plugin/colorbutton_plugin.h"
#include "desktop/widgets/designer_plugin/brushpreview_plugin.h"
#include "desktop/widgets/designer_plugin/groupedtoolbutton_plugin.h"
#include "desktop/widgets/designer_plugin/filmstrip_plugin.h"
#include "desktop/widgets/designer_plugin/resizer_plugin.h"
#include "desktop/widgets/designer_plugin/tablettester_plugin.h"
#include "desktop/widgets/designer_plugin/spinner_plugin.h"
#include "desktop/widgets/designer_plugin/presetselector_plugin.h"
#include "desktop/widgets/designer_plugin/modifierkeys_plugin.h"

DrawpileWidgetCollection::DrawpileWidgetCollection(QObject *parent) :
	QObject(parent)
{
    widgets
		<< new ColorButtonPlugin(this)
		<< new BrushPreviewPlugin(this)
		<< new GroupedToolButtonPlugin(this)
		<< new FilmstripPlugin(this)
		<< new ResizerPlugin(this)
		<< new TabletTesterPlugin(this)
		<< new SpinnerPlugin(this)
		<< new PresetSelectorPlugin(this)
		<< new ModifierKeysPlugin(this)
		;
}

QList<QDesignerCustomWidgetInterface *> DrawpileWidgetCollection::customWidgets() const
{
    return widgets;
}
