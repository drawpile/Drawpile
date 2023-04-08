// SPDX-License-Identifier: GPL-3.0-or-later

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
