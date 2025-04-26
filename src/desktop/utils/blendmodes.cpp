// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/utils/blendmodes.h"
#include <QComboBox>
#include <QCoreApplication>
#include <QStandardItemModel>


int searchBlendModeComboIndex(QComboBox *combo, int mode)
{
	int blendModeCount = combo->count();
	for(int i = 0; i < blendModeCount; ++i) {
		if(combo->itemData(i).toInt() == mode) {
			return i;
		}
	}

	int alphaPreservingMode = canvas::blendmode::toAlphaPreserving(mode);
	if(alphaPreservingMode != mode) {
		for(int i = 0; i < blendModeCount; ++i) {
			if(combo->itemData(i).toInt() == alphaPreservingMode) {
				return i;
			}
		}
	}

	int alphaAffectingMode = canvas::blendmode::toAlphaAffecting(mode);
	if(alphaAffectingMode != mode) {
		for(int i = 0; i < blendModeCount; ++i) {
			if(combo->itemData(i).toInt() == alphaAffectingMode) {
				return i;
			}
		}
	}

	return -1;
}


namespace {

static constexpr int RECOLOR_ENABLED = 0;
static constexpr int RECOLOR_DISABLED = 1;
static constexpr int RECOLOR_OMITTED = 2;

static void addSeparator(QStandardItemModel *model)
{
	QStandardItem *separator = new QStandardItem();
	separator->setAccessibleDescription(QStringLiteral("separator"));
	separator->setData(-1, Qt::UserRole);
	separator->setFlags(
		separator->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
	model->appendRow(separator);
}

static void addBlendModesTo(
	QStandardItemModel *model, int recolor,
	const QVector<canvas::blendmode::Named> &modes)
{
	for(int i = 0, count = modes.size(); i < count; ++i) {
		const canvas::blendmode::Named &m = modes[i];
		if(m.separatorAbove && i != 0) {
			addSeparator(model);
		}

		QStandardItem *item = new QStandardItem(m.name);
		item->setData(int(m.mode), Qt::UserRole);
		if(m.mode == DP_BLEND_MODE_RECOLOR) {
			if(recolor == RECOLOR_DISABLED) {
				item->setEnabled(false);
				model->appendRow(item);
			} else if(recolor != RECOLOR_OMITTED) {
				model->appendRow(item);
			}
		} else {
			model->appendRow(item);
		}

		if(m.separatorBelow && i != count - 1) {
			addSeparator(model);
		}
	}
}

static void addLayerBlendModesTo(QStandardItemModel *model, int recolor)
{
	addBlendModesTo(model, recolor, canvas::blendmode::layerModeNames());
}

static void addGroupBlendModesTo(QStandardItemModel *model, int recolor)
{
	QStandardItem *item = new QStandardItem(QCoreApplication::translate(
		"dialogs::LayerProperties", "Pass Through"));
	item->setData(-1, Qt::UserRole);
	model->appendRow(item);
	addLayerBlendModesTo(model, recolor);
}

static QStandardItemModel *layerBlendModes()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addLayerBlendModesTo(model, RECOLOR_ENABLED);
	}
	return model;
}

QStandardItemModel *layerBlendModesRecolorDisabled()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addLayerBlendModesTo(model, RECOLOR_DISABLED);
	}
	return model;
}

QStandardItemModel *layerBlendModesRecolorOmitted()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addLayerBlendModesTo(model, RECOLOR_OMITTED);
	}
	return model;
}

QStandardItemModel *groupBlendModes()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addGroupBlendModesTo(model, RECOLOR_ENABLED);
	}
	return model;
}

QStandardItemModel *groupBlendModesRecolorDisabled()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addGroupBlendModesTo(model, RECOLOR_DISABLED);
	}
	return model;
}

QStandardItemModel *groupBlendModesRecolorOmitted()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addGroupBlendModesTo(model, RECOLOR_OMITTED);
	}
	return model;
}

QStandardItemModel *brushBlendModes()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_ENABLED, canvas::blendmode::brushModeNames());
	}
	return model;
}

QStandardItemModel *brushBlendModesRecolorOmitted()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_OMITTED, canvas::blendmode::brushModeNames());
	}
	return model;
}

QStandardItemModel *brushBlendModesErase()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_ENABLED, canvas::blendmode::eraserModeNames());
	}
	return model;
}

QStandardItemModel *fillBlendModes()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_ENABLED, canvas::blendmode::pasteModeNames());
	}
	return model;
}

QStandardItemModel *fillBlendModesRecolorOmitted()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_OMITTED, canvas::blendmode::pasteModeNames());
	}
	return model;
}

}

QStandardItemModel *
getLayerBlendModesFor(bool group, bool clip, bool automaticAlphaPreserve)
{
	if(automaticAlphaPreserve) {
		if(clip) {
			return group ? groupBlendModesRecolorDisabled()
						 : layerBlendModesRecolorDisabled();
		} else {
			return group ? groupBlendModes() : layerBlendModes();
		}
	} else {
		return group ? groupBlendModesRecolorOmitted()
					 : layerBlendModesRecolorOmitted();
	}
}

QStandardItemModel *
getBrushBlendModesFor(bool erase, bool automaticAlphaPreserve)
{
	return erase					? brushBlendModesErase()
		   : automaticAlphaPreserve ? brushBlendModes()
									: brushBlendModesRecolorOmitted();
}

QStandardItemModel *getFillBlendModesFor(bool automaticAlphaPreserve)
{
	return automaticAlphaPreserve ? fillBlendModes()
								  : fillBlendModesRecolorOmitted();
}
