// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/utils/blendmodes.h"
#include <QComboBox>
#include <QCoreApplication>
#include <QStandardItemModel>


namespace {
static bool isMatchingBlendMode(QAbstractItemModel *model, int i, int mode)
{
	QModelIndex idx = model->index(i, 0);
	return idx.isValid() && model->flags(idx).testFlag(Qt::ItemIsEnabled) &&
		   model->data(idx, Qt::UserRole).toInt() == mode;
}
}

int searchBlendModeComboIndex(QComboBox *combo, int mode)
{
	QAbstractItemModel *model = combo->model();
	if(model) {
		int blendModeCount = combo->count();
		for(int i = 0; i < blendModeCount; ++i) {
			if(isMatchingBlendMode(model, i, mode)) {
				return i;
			}
		}

		int alphaPreservingMode = canvas::blendmode::toAlphaPreserving(mode);
		if(alphaPreservingMode != mode) {
			for(int i = 0; i < blendModeCount; ++i) {
				if(isMatchingBlendMode(model, i, alphaPreservingMode)) {
					return i;
				}
			}
		}

		int alphaAffectingMode = canvas::blendmode::toAlphaAffecting(mode);
		if(alphaAffectingMode != mode) {
			for(int i = 0; i < blendModeCount; ++i) {
				if(isMatchingBlendMode(model, i, alphaAffectingMode)) {
					return i;
				}
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
	QStandardItemModel *model, int recolor, bool compatibilityMode,
	bool myPaint, const QVector<canvas::blendmode::Named> &modes)
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

		if(compatibilityMode &&
		   !(myPaint ? m.myPaintCompatible : m.compatible)) {
			item->setEnabled(false);
		}

		if(m.separatorBelow && i != count - 1) {
			addSeparator(model);
		}
	}
}

static void addLayerBlendModesTo(
	QStandardItemModel *model, int recolor, bool compatibilityMode)
{
	addBlendModesTo(
		model, recolor, compatibilityMode, false,
		canvas::blendmode::layerModeNames());
}

static void addGroupBlendModesTo(
	QStandardItemModel *model, int recolor, bool compatibilityMode)
{
	QStandardItem *item = new QStandardItem(QCoreApplication::translate(
		"dialogs::LayerProperties", "Pass Through"));
	item->setData(-1, Qt::UserRole);
	model->appendRow(item);
	addLayerBlendModesTo(model, recolor, compatibilityMode);
}

static QStandardItemModel *layerBlendModes()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addLayerBlendModesTo(model, RECOLOR_ENABLED, false);
	}
	return model;
}

QStandardItemModel *layerBlendModesRecolorDisabled()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addLayerBlendModesTo(model, RECOLOR_DISABLED, false);
	}
	return model;
}

QStandardItemModel *layerBlendModesRecolorOmitted()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addLayerBlendModesTo(model, RECOLOR_OMITTED, false);
	}
	return model;
}

QStandardItemModel *layerBlendModesCompat()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addLayerBlendModesTo(model, RECOLOR_ENABLED, true);
	}
	return model;
}

QStandardItemModel *groupBlendModes()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addGroupBlendModesTo(model, RECOLOR_ENABLED, false);
	}
	return model;
}

QStandardItemModel *groupBlendModesRecolorDisabled()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addGroupBlendModesTo(model, RECOLOR_DISABLED, false);
	}
	return model;
}

QStandardItemModel *groupBlendModesRecolorOmitted()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addGroupBlendModesTo(model, RECOLOR_OMITTED, false);
	}
	return model;
}

QStandardItemModel *groupBlendModesCompat()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addGroupBlendModesTo(model, RECOLOR_ENABLED, true);
	}
	return model;
}

QStandardItemModel *brushBlendModes()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_ENABLED, false, false,
			canvas::blendmode::brushModeNames());
	}
	return model;
}

QStandardItemModel *brushBlendModesRecolorOmitted()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_OMITTED, false, false,
			canvas::blendmode::brushModeNames());
	}
	return model;
}

QStandardItemModel *brushBlendModesErase()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_ENABLED, false, false,
			canvas::blendmode::eraserModeNames());
	}
	return model;
}

QStandardItemModel *brushBlendModesCompat()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_ENABLED, true, false,
			canvas::blendmode::brushModeNames());
	}
	return model;
}

QStandardItemModel *brushBlendModesEraseCompat()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_ENABLED, true, false,
			canvas::blendmode::eraserModeNames());
	}
	return model;
}

QStandardItemModel *brushBlendModesMyPaintCompat()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_ENABLED, true, true,
			canvas::blendmode::brushModeNames());
	}
	return model;
}

QStandardItemModel *brushBlendModesEraseMyPaintCompat()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_ENABLED, true, true,
			canvas::blendmode::eraserModeNames());
	}
	return model;
}

QStandardItemModel *fillBlendModes()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_ENABLED, false, false,
			canvas::blendmode::pasteModeNames());
	}
	return model;
}

QStandardItemModel *fillBlendModesRecolorOmitted()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_OMITTED, false, false,
			canvas::blendmode::pasteModeNames());
	}
	return model;
}

QStandardItemModel *fillBlendModesCompat()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_ENABLED, true, false,
			canvas::blendmode::pasteModeNames());
	}
	return model;
}

}

QStandardItemModel *getLayerBlendModesFor(
	bool group, bool clip, bool automaticAlphaPreserve, bool compatibilityMode)
{
	if(compatibilityMode) {
		return group ? groupBlendModesCompat() : layerBlendModesCompat();
	} else if(automaticAlphaPreserve) {
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

QStandardItemModel *getBrushBlendModesFor(
	bool erase, bool automaticAlphaPreserve, bool compatibilityMode,
	bool myPaint)
{
	if(compatibilityMode) {
		if(myPaint) {
			return erase ? brushBlendModesEraseMyPaintCompat()
						 : brushBlendModesMyPaintCompat();
		} else {
			return erase ? brushBlendModesEraseCompat()
						 : brushBlendModesCompat();
		}
	} else if(erase) {
		return brushBlendModesErase();
	} else if(automaticAlphaPreserve) {
		return brushBlendModes();
	} else {
		return brushBlendModesRecolorOmitted();
	}
}

QStandardItemModel *
getFillBlendModesFor(bool automaticAlphaPreserve, bool compatibilityMode)
{
	if(compatibilityMode) {
		return fillBlendModesCompat();
	} else if(automaticAlphaPreserve) {
		return fillBlendModes();
	} else {
		return fillBlendModesRecolorOmitted();
	}
}
