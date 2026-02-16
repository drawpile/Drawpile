// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_ENUMS_H
#define LIBCLIENT_TOOLS_ENUMS_H
#include <QFlags>

namespace tools {

enum class ColorPickSource { Canvas, Tool, Touch, Adjust };
enum class DeviceType { Mouse, Tablet, Touch };
enum class QuickAdjustType { Tool1, Tool2, Tool3, ColorH, ColorS, ColorV };
enum class ToolState { Normal, Busy };
enum class Capability : unsigned int {
	AllowColorPick = (1u << 0u),
	HandlesRightClick = (1u << 1u),
	Fractional = (1u << 2u),
	SnapsToPixel = (1u << 3u),
	SupportsPressure = (1u << 4u),
	IgnoresSelections = (1u << 5u),
	SendsNoMessages = (1u << 6u),
	AllowToolAdjust1 = (1u << 7u),
	AllowToolAdjust2 = (1u << 8u),
	AllowToolAdjust3 = (1u << 9u),
};
Q_DECLARE_FLAGS(Capabilities, Capability)

enum class TabletInputMode : int {
	Uninitialized, // Must be the first value, used for a range check.
	KisTabletWinink,
	KisTabletWintab,
	KisTabletWintabRelativePenHack,
	// Only Qt6 allows for toggling between Windows Ink and Wintab (barely,
	// through private headers), Qt5 always uses Windows Ink if it's available.
	Qt6Winink,
	Qt6Wintab,
	Qt5,
	KisTabletWininkNonNative,
	Last = KisTabletWininkNonNative, // Must be equal to the last value, used
									 // for a range check.
};

enum class EraserAction {
	Ignore,
#if !defined(__EMSCRIPTEN__) && !defined(Q_OS_ANDROID)
	Switch,
#endif
	Override,
#if defined(__EMSCRIPTEN__) || defined(Q_OS_ANDROID)
	Default = Override,
#else
	Default = Switch,
#endif
};

enum class RotationMode {
	Normal,
	NoSnap,
	Discrete,
	Count,
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(tools::Capabilities)

#endif
