// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PAINTCORE_BLENDMODES_H
#define PAINTCORE_BLENDMODES_H

extern "C" {
#include <dpmsg/blend_mode.h>
}

#include <QString>
#include <QVector>
#include <QPair>

class QStandardItemModel;

namespace canvas {
namespace blendmode {
	struct Named {
		DP_BlendMode mode;
		QString name;
	};

	QString translatedName(int mode);

	//! Get the SVG name for the given blend mode
	QString svgName(DP_BlendMode mode);

	//! Find a blend mode by its SVG name
	DP_BlendMode fromSvgName(
		const QString &name, DP_BlendMode defaultMode = DP_BLEND_MODE_NORMAL);

	//! Get a list of brush blend modes and their translated names
	QVector<Named> brushModeNames();

	//! Get a list of eraser brush blend modes and their translated names
	QVector<Named> eraserModeNames();

	//! Get a list of layer blend modes and their translated names
	QVector<Named> layerModeNames();

	//! Get a list of paste/fill blend modes and their translated names
	QVector<Named> pasteModeNames();

	bool isValidBrushMode(DP_BlendMode mode);
	bool isValidEraseMode(DP_BlendMode mode);

	//! Is this blend mode compatible with Drawpile 2.1?
	bool isBackwardCompatibleMode(DP_BlendMode mode);

	//! Disables incompatible blend modes, mode must be in the UserRole data.
	void setCompatibilityMode(QStandardItemModel *model, bool compatibilityMode);
}
}

#endif

