// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_CANVAS_BLENDMODES_H
#define LIBCLIENT_CANVAS_BLENDMODES_H
extern "C" {
#include <dpmsg/blend_mode.h>
}
#include <QPair>
#include <QString>
#include <QVector>

class QStandardItemModel;

namespace canvas {
namespace blendmode {
struct Named {
	DP_BlendMode mode;
	QString name;
	bool separatorAbove;
	bool separatorBelow;
	bool compatible;
	bool myPaintCompatible;
};

//! Get the SVG name for the given blend mode
QString oraName(DP_BlendMode mode);

//! Find a blend mode by its SVG name
DP_BlendMode fromOraName(
	const QString &name, DP_BlendMode defaultMode = DP_BLEND_MODE_NORMAL);

//! Get a list of brush blend modes and their translated names
QVector<Named> brushModeNames();

//! Get a list of eraser brush blend modes and their translated names
QVector<Named> eraserModeNames();

//! Get a list of layer blend modes and their translated names
QVector<Named> layerModeNames();

//! Get a list of paste/fill blend modes and their translated names
QVector<Named> pasteModeNames();

//! Get a list of blend modes and translated names assignable to shortcuts.
QVector<Named> shortcutModeNames();

bool isValidBrushMode(int mode);
bool isValidEraseMode(int mode);

bool preservesAlpha(int mode);
bool presentsAsEraser(int mode);
bool presentsAsAlphaPreserving(int mode);
bool comparesAlpha(int mode);
bool directOnly(int mode);

bool alphaPreservePair(
	int mode, DP_BlendMode *outAlphaAffecting,
	DP_BlendMode *outAlphaPreserving);
int toAlphaAffecting(int mode);
int toAlphaPreserving(int mode);
int toCompatible(int mode);

void adjustAlphaBehavior(int &mode, bool preserveAlpha);

bool isCompatible(int mode, bool myPaint);

}

namespace paintmode {

bool isValidPaintMode(int mode);

QString settingName(DP_PaintMode mode);

DP_PaintMode fromSettingName(const QString &name, DP_PaintMode defaultMode);

}
}

#endif
