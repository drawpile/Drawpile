// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_UTILS_BLENDMODES_H
#define DESKTOP_UTILS_BLENDMODES_H
#include "libclient/canvas/blendmodes.h"

class QComboBox;
class QStandardItemModel;


int searchBlendModeComboIndex(QComboBox *combo, int mode);

QStandardItemModel *getLayerBlendModesFor(
	bool group, bool clip, bool automaticAlphaPreserve, bool compatibilityMode);

QStandardItemModel *getBrushBlendModesFor(
	bool erase, bool automaticAlphaPreserve, bool compatibilityMode);

QStandardItemModel *
getFillBlendModesFor(bool automaticAlphaPreserve, bool compatibilityMode);

#endif
